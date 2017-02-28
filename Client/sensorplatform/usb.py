# USB SensorPlatform device interface
# Copyright (C) 2016-2017 Michael Sparmann
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


import usb.core
import struct
import threading
import queue
import binascii



# Base class which abstracts access to a USB device implementing the SensorPlatform USB protocol
class USBDevice(object):

    # Constructor: Find the first present SensorPlatform USB device
    def __init__(self):
        # Initialize object state:
        self.type = None  # Cached interface type tuple (SubClass, Protocol)
        self.outEp = None  # Cached bulk OUT endpoint of the device
        self.inEp = None  # Cached bulk IN endpoint of the device
        self.printUSBPackets = False  # Debug: Set this to true to print all USB bulk packets
        self.rxDataQueue = queue.Queue()  # Received USB bulk packet queue
        self.commLock = threading.Lock()  # Communication lock, protects the following variables
        self.replyListener = [None] * 256  # Reply received events for command sequence numbers
        self.replyPacket = [None] * 256  # Reply packet data for command sequence numbers
        self.cmdFinished = threading.Condition(self.commLock)  # Notified when a sequence number is freed
        self.activeListeners = 0  # Number of command sequence numbers being in use
        self.nextSeq = 1  # Next command sequence number to use (if free)
        
        # Find the first present USB device with SensorPlatform VID/PID
        self.dev = usb.core.find(idVendor=0xf055, idProduct=0x5053)
        if self.dev is None: raise Exception("Cannot find any SensorPlatform devices")
        
        # Find the first vendor-specific interface with SubClass 0x5X:
        #     0x52: SensorPlatform Receiver
        #     0x53: SensorPlatform Sensor Node
        for intf in self.dev.get_active_configuration().interfaces():
            if intf.bInterfaceClass == 0xff and intf.bInterfaceSubClass >> 4 == 0x5:
                # Cache the exact interface type and its bulk endpoints
                self.type = (intf.bInterfaceSubClass, intf.bInterfaceProtocol)
                self.outEp = intf.endpoints()[0]
                self.inEp = intf.endpoints()[1]
                break
        if self.outEp is None or self.inEp is None:
            raise Exception("Failed to get SensorPlatform endpoints")
            
        # Launch USB packet receiver thread (which puts packets into rxDataQueue)
        threading.Thread(daemon=True, target=self.rxThread).start()
        # Launch USB packet processor thread (which processes packets from rxDataQueue)
        threading.Thread(daemon=True, target=self.procThread).start()

        
    # USB data receiver thread: Read data from the device and put it into rxDataQueue
    def rxThread(self):
        while True:
            # Attempt to read up to 64KiB of data (1024 packet) within up to 10 seconds.
            # The USB device will terminate this early using a zero-length packet once it has
            # no more data to send. A smaller timeout would allow for earlier processing of
            # the received packets, however there is a bug in the linux kernel which causes
            # data to be lost in the timeout case, so we want to avoid timeouts.
            # A smaller transfer size (must be a multiple of 64!) might also help with quick
            # processing of the received data, but increases the risk of device-side buffer
            # overflows because we can receive less data without restarting the transfer.
            # (Some CPU-heavy operations such as data serialization cause the python
            # interpreter's global interpreter lock to be unavailable for tens of
            # milliseconds at once, preventing this loop from spinning quickly enough.)
            try: self.rxDataQueue.put(bytes(self.inEp.read(65536, 10000)))
            except: pass


    # Received USB packet processor thread: Processes packets from rxDataQueue
    def procThread(self):
        while True:
            # Grab an element from the queue and split it into packets
            data = self.rxDataQueue.get()
            if len(data) < 64: continue  # Should never actually happen
            for packet in [data[i:i+64] for i in range(0, len(data), 64)]:
                # Dump the received packet if requested
                if self.printUSBPackets: print("  USB <<< " + self.hex(packet))
                # Parse the header
                msg, seq, reserved = struct.unpack("<HBB", packet[:4])
                if msg >> 14 == 2 and self.replyListener[seq] is not None:
                    # This is a reply packet. Extract the result code, put it into the
                    # corresponding listener's mailbox and wake up that listener.
                    result = struct.unpack("<I", packet[4:8])[0]
                    self.replyPacket[seq] = (msg, seq, reserved, result, packet[8:])
                    self.replyListener[seq].set()
                if msg >> 14 == 3:
                    # This is a notify packet (usually about a received radio packet),
                    # so pass it to the notify packet handler (implemented by a subclass)
                    self.handleNotify(packet)
                  

    # Send a USB packet to the device
    def sendPacket(self, packet):
        # Dump the packet if requested
        if self.printUSBPackets: print("  USB >>> " + self.hex(packet))
        # Attempt to send the packet (padded to 64 bytes) with a timeout of 1 second
        if self.outEp.write(packet.ljust(64, b"\0"), 1000) != 64:
            raise Exception("USB write failed")
            

    # Asynchronous command initiation: Send a command and set up reply listener
    def asyncCmd(self, id, payload=b"", timeout=1):
        with self.commLock:
            # Acquire a free command sequence number
            while True:
                while self.activeListeners > 192:
                    # Too many commands in progress, wait for one to finish
                    if not self.cmdFinished.wait(timeout):
                        raise Exception("Timeout acquiring command sequence number")
                seq = self.nextSeq
                self.nextSeq = (self.nextSeq + 1) & 0xff  # Increment and wrap around if necessary
                if self.nextSeq == 0: self.nextSeq = 1  # Avoid sequence number 0
                if self.replyListener[seq] is None:
                    # We found a free sequence number, let's use that.
                    self.activeListeners += 1
                    break
                # That sequence number is in use, try the next one (next loop iteration)
            # Set up a reply event listener, which also marks the sequence number as in use
            self.replyListener[seq] = threading.Event()
        # Send the command packet with the acquired sequence number
        self.sendPacket(struct.pack("<HBB", id, seq, 0) + payload)
        # Return the sequence number for the caller to keep track of its progress
        return seq

        
    # Check if an asynchronous command has received a reply
    def isCmdDone(self, seq):
        return self.replyListener[seq].is_set()
    
    
    # Cancel a running asynchronous command, or clean up after a finished one
    def cancelCmd(self, seq):
        with self.commLock:
            # Just pretend that the command was never sent. We could cause a spurious response
            # for a future command that way, but this is only called after we have received the
            # response or after a substantial timeout has expired (and it's unlikely that we'll
            # ever get a response.) We also keep 63 recently used sequence numbers free to
            # further reduce the likelyhood of a spurious response hitting an active listener.
            self.replyListener[seq] = None
            self.replyPacket[seq] = None
            self.activeListeners -= 1
            self.cmdFinished.notify_all()
    
    
    # Collect the response of an asynchronous command
    def finishCmd(self, seq, timeout=1):
        try:
            # If this command isn't finished yet, wait for it to be finished or a timeout.
            if not self.replyListener[seq].wait(timeout):
                raise Exception("Timeout waiting for command response")
            # The command is finished now, return the response
            return self.replyPacket[seq]
        # Whether we got a response or not, remove the
        # response listener and free the sequence number.
        finally: self.cancelCmd(seq)


    # Synchronous (blocking) command execution
    def cmd(self, id, payload=b"", timeout=1):
        # If the command doesn't expect a response (timeout == 0), just send it and return
        if timeout == 0: return self.sendPacket(struct.pack("<HBB", id, 0, 0) + payload)
        # Otherwise just wrap the asynchronous flow together
        return self.finishCmd(self.asyncCmd(id, payload, timeout), timeout)
            

    # Convenience short hand function to hex-encode a binary string for dumping
    def hex(self, data): return binascii.hexlify(data).decode("ascii")
