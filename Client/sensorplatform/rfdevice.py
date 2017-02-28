# Radio-connected SensorPlatform device interface
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


import time
import struct
import threading



# Device protocol and firmware information structure (not required for addressing)
class RfDevInfo(object):
    def __init__(self, data):
        self.protoVendor, self.protoType, self.protoVersion = struct.unpack("<IHH", data[:8])
        self.fwVendor, self.fwType, self.fwVersion = struct.unpack("<IHH", data[8:])

        

# Device hardware unique ID structure (for addressing, optionally including RFDevInfo if known)
class RFDevID(object):
    # Ensure that RFDevID instances referring to the same device are considered equal
    def __hash__(self): return self._hash
    def __eq__(self, o): return (self.vendor, self.product, self.serial) == (o.vendor, o.product, o.serial)
    def __ne__(self, o): return not(self == o)

    # Initialize structure from binary device information data
    # (may optionally include RFDevInfo part)
    def __init__(self, data):
        self.vendor, self.product, self.serial = struct.unpack("<III", data[:12])
        self.idstr = "%08X%08X%08X" % (self.vendor, self.product, self.serial)
        self._hash = self.vendor ^ self.product ^ self.serial
        self.info = None
        self.update(data)
        
    # Update RFDevInfo using binary device information data
    def update(self, data):
        if self.info is None and len(data) > 12: self.info = RfDevInfo(data[12:])
        
    # Returns binary representation of the device hardware unique ID
    def binary(self):
        return struct.pack("<III", self.vendor, self.product, self.serial)

        

# Base class which abstracts access to a SensorPlatform device connected via a radio link
class RFDevice(object):

    # Constructor: Initialize object state
    def __init__(self, manager, id):
        self.commLock = threading.Lock()  # Lock that protects most of the other variables
        self.cmdFinished = threading.Condition(self.commLock)  # Notified when a sequence number is freed
        self.packetReceived = threading.Condition(self.commLock)  # Notified when any packet was received
        self.nextSeq = 0  # Next command sequence number to use (if free)
        self.cmdData = [None] * 32  # Command packet data for each sequence number (for retransmissions)
        self.replyListener = [None] * 32  # Reply received events for command sequence numbers
        self.replyPacket = [None] * 32  # Reply packet data for command sequence numbers
        self.pendingRx = [0] * 32  # How many retransmissions of the command have not yet received a reply
        self.lastTx = [0] * 32  # Time of the last transmission attempt of the command packet
        self.lastRx = 0  # Time at which the last packet (of any kind) from the device was received
        self.lastNoData = 0  # Time at which the last buffer empty status packet was received from the device
        self.activeListeners = 0  # Number of command sequence numbers being in use
        self.curTelemetry = None  # Most recently captured set of telemetry counter values
        self.lastTelemetry = None  # Telemetry counter state after last snapshotTelemetry
        self.telemetryDelta = None  # Delta per second between the last snapshotTelemetry calls
        self.bitrate = 0  # Estimated average data bit rate as reported by the device (unused)
        self.dataSeq = 0  # Full sequence number of the last received data packet
        self.manager = manager  # Pointer to the RFManager that this device is connected through
        self.id = id  # Device unique hardware ID, protocol and firmware information (RFDevID object)
        self.drop = False  # Whether the device connection was dropped due to a communication timeout

        
    # Determine the change rate per second of the telemetry counters.
    # Call this every <interval> seconds.
    def snapshotTelemetry(self, interval):
        if self.curTelemetry is None: return
        if self.lastTelemetry is not None:
            # We have two telemetry snapshots and can capture deltas
            for i in range(len(self.curTelemetry)):
                # Handle wraparound and divide by the time delta (in seconds) between the snapshots
                self.telemetryDelta[i] = ((self.curTelemetry[i] - self.lastTelemetry[i]) & 0xffff) / interval
        else: self.telemetryDelta = [0] * len(self.curTelemetry)
        # Save current counter values for next delta calculation
        self.lastTelemetry = self.curTelemetry


    # Asynchronous command initiation: Send a command and set up reply listener
    def asyncCmd(self, cmd, arg, payload=b"", timeout=10):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: raise Exception("Device dropped")
        with self.commLock:
            # Acquire a free command sequence number
            seq = None
            while seq is None:
                poll = False
                while self.activeListeners >= len(self.replyListener):
                    # Too many commands in progress, wait for one to finish
                    if not self.cmdFinished.wait(timeout):
                        raise Exception("Timeout acquiring command sequence number for device %08X" % self.id.serial)
                for _ in self.replyListener:
                    self.nextSeq = (self.nextSeq + 1) & 0x1f  # Increment and wrap around if necessary
                    if self.replyListener[self.nextSeq] is None:
                        # If the last no data info packet arrived significantly after sending the
                        # last command packet with this sequence number, we can assume that no
                        # more replies to the old command will arrive with this sequence number.
                        # Note that the delta needs to include the maximum command processing time.
                        if self.lastNoData - self.lastTx[self.nextSeq] > 0.05:
                            self.pendingRx[self.nextSeq] = 0
                        # If we received as many responses as we sent command packets,
                        # we also know that there will be no more responses to the old command.
                        if self.pendingRx[self.nextSeq] > 0: poll = True
                        else:
                            # We found a free sequence number, let's use that.
                            seq = self.nextSeq
                            break
                        # We aren't sure if this sequence number might still get some
                        # response packets for an old command. Try to find another one.
                    # That sequence number is in use, try the next one (next loop iteration)
                if seq is None and poll and time.monotonic() - self.lastRx > 0.02:
                    # We didn't find a free sequence number, and there was some time since the
                    # last communication with the device. Poll it to figure out if its buffers
                    # are empty. If they are, we can likely free up some sequence numbers which
                    # are just waiting for duplicate responses.
                    self.manager.pollDevice(self)
                    # Wait for a bit before scanning all sequence numbers again
                    self.packetReceived.wait(0.01)
            # We got a sequence number. Set up a reply event listener,
            # which also marks the sequence number as in use.
            self.activeListeners += 1
            self.replyListener[seq] = threading.Event()
            self.pendingRx[seq] = 0
        # Build the command packet. It will be sent by the subsequent call to cmdAttempt.
        self.cmdData[seq] = struct.pack("<HBB", cmd, arg, seq) + payload
        # Return the sequence number for the caller to keep track of its progress
        return seq
        

    # Attempt a (re-)transmission of a command packet
    def cmdAttempt(self, seq):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: raise Exception("Device dropped")
        with self.commLock:
            # Increment outstanding reply counter and capture last transmission attempt timestamp
            data = self.cmdData[seq]
            self.pendingRx[seq] += 1
            self.lastTx[seq] = time.monotonic()
        # Transmit the packet to the device
        self.manager.sendPacket(self, data)
        # Trigger polling the device for a reply
        self.manager.pollDevice(self)

        
    # Check if an asynchronous command has received a reply
    def isCmdDone(self, seq):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: raise Exception("Device dropped")
        return self.replyListener[seq].is_set()
    
    
    # Cancel a running asynchronous command, or clean up after a finished one
    def cancelCmd(self, seq):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: raise Exception("Device dropped")
        with self.commLock:
            # Remove the listener for this sequence number, but don't reset the transmission count
            # and time. Those will be checked to ensure that spurious responses (intended for the
            # old command) are unlikely before allocating that sequence number again.
            self.cmdData[seq] = None
            self.replyListener[seq] = None
            self.replyPacket[seq] = None
            self.activeListeners -= 1
            self.cmdFinished.notify()
    
    
    # Collect the response of an asynchronous command. If there is none,
    # attempt to retransmit the command the specified number of times in the specified interval.
    def finishCmd(self, seq, timeout=0.1, retries=64):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: raise Exception("Device dropped")
        try:
            lastError = None
            for i in range(retries):
                try:
                    # If we don't have a response yet, (re-)transmit the command
                    if not self.isCmdDone(seq): self.cmdAttempt(seq)
                    # Wait for a response to arrive or the timeout to expire
                    if self.replyListener[seq].wait(timeout):
                        # We got a response. Return it.
                        return self.replyPacket[seq]
                    # There was a timeout. Resend the command if there are still attempts left.
                except Exception as e:
                    # There was some error (usually no route to device).
                    # Keep track of what exactly it was to report it later.
                    lastError = e
                    # Wait for one retransmission interval before trying again.
                    time.sleep(timeout)
            # We failed to get a response. If we got any exception while attempting to
            # transmit the command, report that. Otherwise report a timeout error.
            if lastError is not None: raise lastError
            raise Exception("Timeout waiting for command response from device %08X" % self.id.serial)
        # Whether we got a response or not, remove the
        # response listener and free the sequence number.
        finally: self.cancelCmd(seq)

            
    # Synchronous (blocking) command execution
    def cmd(self, cmd, arg, payload=b"", starttimeout=10, resptimeout=0.1, retries=64):
        # Just wrap the asynchronous flow together
        return self.finishCmd(self.asyncCmd(cmd, arg, payload, starttimeout), resptimeout, retries)
        

    # Check whether the result of a command was success, and if not, report the error.
    # If the command was successful, pass through the result.
    def check(self, args):
        if args[0] != 0: raise Exception("Operation on device %08X returned status %02X" % (self.id.serial, args[0]))
        return args

    
    # Handle incoming radio packets from this device
    def handlePacket(self, sofCount, packet):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: return
        with self.commLock:
            # Notify other threads about reception of a packet and
            # keep track of the last successful communication time with the device.
            self.lastRx = time.monotonic()
            self.packetReceived.notify_all()
            if packet[3] == 0xff:
                # This is a no data info packet, signalling that the device didn't have
                # any data to transmit at the time that it was polled. The receiver device
                # will automatically stop polling it for the amount of time that it requests.
                # Keep track of the last time when the device reported an empty buffer
                # and capture the status and telemetry information contained in the packet.
                self.lastNoData = self.lastRx
                self.bitrate, self.dataSeq = struct.unpack("<II", packet[8:16])
                self.curTelemetry = (sofCount, *struct.unpack("<HHHHHHHH", packet[16:]))
            elif packet[3] >> 7 == 0:
                # This is a measurement data stream packet. Figure out its sequence number.
                seq = struct.unpack("<H", packet[2:4])[0]
                # As we only get the low 15 bits, determine the delta to our last known
                # sequence number for this device in the direction where it is closest.
                delta = (seq - self.dataSeq) & 0x7fff
                # If that delta is negative, sign-extend it to reflect the correct number.
                if delta & 0x4000: delta -= 0x8000
                # Move the last known sequence number by that delta, so that we get the
                # (likely) current (full) sequence number on the device side.
                self.dataSeq += delta
                # Pass the packet, along with its frame number and sequence number information,
                # to the received data packet handler (implemented by a subclass)
                self.handleDataPacket(sofCount, self.dataSeq, packet[4:])
            elif packet[3] >> 5 == 4:
                # This is a command response packet. Figure out the sequence number from the packet.
                seq = packet[3] & 0x1f
                # Decrement the outstanding response counter.
                self.pendingRx[seq] -= 1
                if self.replyListener[seq] is not None:
                    # There is a listener for that sequence number. Put the packet into
                    # the corresponding listener's mailbox and wake up that listener.
                    self.replyPacket[seq] = (packet[2], packet[4:])
                    self.replyListener[seq].set()


    # Dummy function that consumes measurement data packets. Override in a subclass.
    def handleDataPacket(self, frame, seq, data): pass
    
    
    # If we lost track of this device and will (possibly) re-instantiate it,
    # reject any operations that attempt to access the old instance.
    def destroy(self):
        self.drop = True
