# SensorPlatform Receiver device interface
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


import sensorplatform.usb
import time
import random
import struct
import threading



# SensorPlatform Receiver device class, used to handle radio communication
class Receiver(sensorplatform.usb.USBDevice):

    # Constructor: Find the first present SensorPlatform Receiver
    def __init__(self):
        # Initialize base class
        # TODO: Implement proper filtering for the correct device type.
        #       Right now we just assume that the first device that we get is a receiver,
        #       which works well enough as it's the only SensorPlatform device type that
        #       actually implements USB communication so far.
        sensorplatform.usb.USBDevice.__init__(self)
        # Initialize object state:
        self.printRFPackets = False  # Debug: Set this to true to print all radio packets
        self.packetReceivedHook = None  # Radio packet received hook (used by RFManager)
        self.curTelemetry = None  # Most recently captured set of telemetry counter values
        self.lastTelemetry = None  # Telemetry counter state after last snapshotTelemetry
        self.telemetryDelta = None  # Delta per second between the last snapshotTelemetry calls
        self.pollLock = threading.Lock()  # Lock for pollQueue access
        self.pollQueue = []  # List of device NodeIDs that should be polled for packets soon
        # Start node poll request sender thread
        threading.Thread(daemon=True, target=self.pollThread).start()


    # Read current telemetry counter values from the receiver device
    def updateTelemetry(self):
        self.curTelemetry = struct.unpack("<IIIII", self.getRadioStats()[4][4:24])


    # Determine the change rate per second of the telemetry counters.
    # Call this every <interval> seconds and call updateTelemetry immediately before this
    def snapshotTelemetry(self, interval):
        if self.curTelemetry is None: return
        if self.lastTelemetry is not None:
            # We have two telemetry snapshots and can capture deltas
            for i in range(len(self.curTelemetry)):
                # Handle wraparound and divide by the time delta (in seconds) between the snapshots
                self.telemetryDelta[i] = ((self.curTelemetry[i] - self.lastTelemetry[i]) & 0xffffffff) / interval
        else: self.telemetryDelta = [0] * len(self.curTelemetry)  # No delta info, set it zo zero
        # Save current counter values for next delta calculation
        self.lastTelemetry = self.curTelemetry


    # Request telemetry data from the receiver device
    def getRadioStats(self):
        return self.cmd(0x0100)
        

    # Shut down the receiver's radio
    def stopRadio(self):
        return self.cmd(0x0200)
        

    # Configure and start up the receiver's radio
    def startRadio(self, channel, speed=0, txPower=0, receiverTxPower=0, guardBits=0, preGapBits=0, postGapBits=0, netId=None):
        # If NetId was passed as None (or not at all), choose a random one
        if netId is None: netId = random.randrange(256)
        print("Starting radio communication on %d MHz with netId %d..." % (2400 + channel, netId))
        return self.cmd(0x0201, struct.pack("<BBHBBHIBB", channel, netId, preGapBits | (speed << 14), guardBits,
                                                          txPower << 4, 0, 0, postGapBits, receiverTxPower))


    # Enqueue an device NodeId to be polled for packets soon
    def pollDevice(self, target):
        with self.pollLock:
            # Ignore the request if that NodeId is already about to be polled
            if target in self.pollQueue: return
            self.pollQueue.append(target)

        
    # Node poll request sender thread:
    # This thread checks pollQueue every 10ms for RF nodes needing to be polled.
    # If there are some, it will send a request for up to 28 of them to be polled for packets.
    def pollThread(self):
        while True:
            with self.pollLock:
                # Grab the first 28 NodeIDs waiting to be polled
                targets = self.pollQueue[:28]
                self.pollQueue = self.pollQueue[28:]
            # If we got any, send a poll request for them to the receiver
            if len(targets) > 0: self.cmd(0x027e, bytearray(targets), 0)
            # Sleep 10ms
            time.sleep(0.01)


    # Set fixed RX slot assignment (set slots to owner == 0 for auto-assignment)
    def assignSlots(self, slotOwners):
        return self.cmd(0x027f, struct.pack("28B", *slotOwners))


    # Send a radio packet to the specified NodeID
    def sendRFPacket(self, target, packet):
        # Dump the packet if requested
        if self.printRFPackets: print("RF %02X >>> " % target + self.hex(packet))
        return self.cmd(0x0280, struct.pack("B", target).ljust(28, b"\0") + packet, 0)


    # Handle notification (unsolicited) messages from the receiver device
    def handleNotify(self, packet):
        msg, seq, reserved = struct.unpack("<HBB", packet[:4])
        if msg == 0xc001:
            # This is a notification about a received radio packet.
            # Get the packet contents and the current radio frame number.
            sofCount = struct.unpack("<H", packet[4:6])[0]
            data = packet[32:]
            # Dump the packet if requested
            if self.printRFPackets: print("RF 00 <<< %04X " % sofCount + self.hex(data))
            # Call received packet hook to pass the packet to RFManager for further handling
            if self.packetReceivedHook is not None: self.packetReceivedHook(self, sofCount, data)
