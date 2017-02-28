# Radio receiver and remote device manager and routing hub
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

# This code keeps track of receiver devices, devices connected via a radio link,
# and possible communication paths. Manages routing and address assignment
# and can handle remote devices moving between receivers seamlessly.

import sensorplatform.device.mapper
import time
import random
import struct
import binascii
import threading
import queue



# Wrapper class for remote devices connected via a radio link, adding some attributes.
class DeviceData(object):
    def __init__(self, device):
        self.addr = None  # Current route (AddressData) to the device (may be None)
        self.lastAssignAttempt = 0  # Time of the last address assigning attempt for this device
        self.device = device  # The remote device instance
        
        
    # Forget the current route to the device
    def releaseAddr(self):
        if self.addr is None: return
        self.addr.receiver.addrs[self.addr.nodeid] = None
        self.addr = None

        

# Class representing an allocated address on a radio channel.
class AddressData(object):
    def __init__(self, device, receiver, nodeid):
        self.device = device  # The DeviceData that the address is assigned to
        self.receiver = receiver  # The ReceiverData that the device is connected to
        self.nodeid = nodeid  # The device's NodeId on the receiver's channel
        
        
    # Called after successfully communicating with a device. Resets the deassociation timeout.
    def refresh(self):
        self.expires = time.monotonic() + 5
        
        
    # Check if the deassociation timeout has expired and deassociate the device if so
    def expired(self):
        if time.monotonic() > self.expires:
            # The deassociation timeout has expired. The device will have dropped the
            # NodeId by now and will attempt to acquire a new one (if it is still alive)
            self.device.releaseAddr()
            return True
        # The deassociation timeout hasn't expired yet. Report that result.
        return False



# Wrapper class for radio receiver devices, adding some attributes.
class ReceiverData(object):
    def __init__(self, receiver):
        self.addrs = [None] * 101  # NodeId allocations on this receiver's channels
        self.receiver = receiver  # The receiver device instance
        
        
    # Get a free address on the receiver's channel if there are any
    def getFreeAddr(self):
        # Collect all free NodeIds into a list.
        candidates = []
        for nodeid, data in enumerate(self.addrs):
            if nodeid == 0: continue  # NodeId 0 may not be used
            # Calling data.expired() may actually release a NodeId.
            if data is None or data.expired(): candidates.append(nodeid)
        # Raise an exception if there was no free NodeId available
        if len(candidates) == 0: raise Exception("No unused addresses available")
        # Return a random NodeId that is free
        return candidates[random.randrange(len(candidates))]
        
        
    # Assign a NodeId on this receiver's channel to a device
    def assignAddr(self, device):
        # Remove the old route to the device if there was one on a different receiver.
        # It might have moved to this one and the old one might not have noticed that yet.
        if device.addr is not None and device.addr.receiver != self: device.releaseAddr()
        if device.addr is None:
            # Get a free NodeId and register a router for the device on this receiver.
            addr = AddressData(device, self, self.getFreeAddr())
            self.addrs[addr.nodeid] = addr
            device.addr = addr
        # We either have just registered a route for the device on this receiver, or it already
        # had one. Reset the deassociation timeout and inform the device about its new NodeId.
        device.addr.refresh()
        packet = struct.pack("BBBB", 0x7f, 0x80, 0x00, device.addr.nodeid) + device.device.id.binary()
        self.receiver.sendRFPacket(0x7f, packet)
        # Poll the new NodeId for the receiver device to start keeping track of it.
        self.receiver.pollDevice(device.addr.nodeid)
        


# Radio communication routing hub and address assignment manager class
class RFManager(object):

    # Constructor: Initialize object state
    def __init__(self):
        self.printDroppedPackets = False  # Whether to print notifications about packets with unknown origin
        self.printFullyDiscovered = False  # Whether to print notifications about newly discovered devices
        self.newDeviceHook = None  # New device discovered hook
        self.receivers = {}  # Map of all connected receiver objects to their ReceiverData objects
        self.devices = {}  # Map of all known device objects to their DeviceData objects
        self.rfLock = threading.RLock()  # Radio communication lock, protects routing-related information
        self.rxDataQueue = queue.Queue()  # Received radio packet queue
        self.telemetryInterval = 1  # Interval (in seconds) how often to calculate telemetry counter deltas
        # Start the received packet processor thread (processes packets from rxDataQueue)
        threading.Thread(daemon=True, target=self.rxThread).start()
        # Start the telemetry collector thread
        threading.Thread(daemon=True, target=self.telemetryThread).start()

    
    # Add a new receiver device to the routing hub
    def addReceiver(self, receiver):
        # Create a new ReceiverData object for it and register it in the map
        self.receivers[receiver] = ReceiverData(receiver)
        # Clean out any old static time slot assignments (if present)
        receiver.assignSlots([0x00] * 28)
        # Register our received packet hook
        receiver.packetReceivedHook = self.handlePacket
        

    # Called whenever a radio packet is received on any receiver. Puts it into rxDataQueue.
    def handlePacket(self, receiver, sofCount, data):
        self.rxDataQueue.put((self.receivers[receiver], sofCount, data))


    # Received packet processor thread (processes packets from rxDataQueue)
    def rxThread(self):
        while True:
            # Get a packet from the queue (blocks if there is none)
            r, sofCount, data = self.rxDataQueue.get()
            now = time.monotonic()
            if data[0] == 0x7f:  # Notify
                if data[1] == 0x00:  # NodeId
                    if data[3] == 0x00:  # No ID assigned yet
                        with self.rfLock:
                            # This is a device asking us for a NodeId (having none assigned).
                            # Instantiate an RFDevID object with the device's unique hardware ID.
                            id = sensorplatform.rfdevice.RFDevID(data[4:])
                            if not id in self.devices:
                                # This is a device that we haven't seen before. Figure out
                                # what it is, instantiate its driver object and a DeviceData
                                # object wrapping that. Then register it in the device map.
                                self.devices[id] = DeviceData(sensorplatform.device.mapper.instantiate(self, id))
                                # Call the new device detected hook if there is one.
                                if self.newDeviceHook is not None: self.newDeviceHook(id)
                            # If there has already been an attempt to assign a NodeId to
                            # this device within the last 200ms, ignore this request.
                            elif now - self.devices[id].lastAssignAttempt < 0.2: continue
                            # Otherwise attempt to assign a new NodeId and register the route.
                            self.devices[id].lastAssignAttempt = now
                            r.assignAddr(self.devices[id])
            else:
                # Determine which device this packet originates from.
                d = None
                with self.rfLock:
                    if r.addrs[data[0]] is not None:
                        addr = r.addrs[data[0]]
                        addr.refresh()  # Reset the device's deassociation timeout
                        d = addr.device
                # If we found the device, pass the packet to its driver.
                if d is not None: d.device.handlePacket(sofCount, data)
                # Otherwise print a notification about a spurious packet (if requested)
                elif self.printDroppedPackets:
                    print("Dropped packet from unknown nodeId %02x (frame %04X): %s" % (data[0], sofCount, binascii.hexlify(data).decode("ascii")))
                    

    # Telemetry collector thread: Collects an differentiates telemetry counters periodically
    def telemetryThread(self):
        while True:
            time.sleep(self.telemetryInterval)
            with self.rfLock:
                # Capture all receiver telemetry counters
                for r in self.receivers.values():
                    r.receiver.updateTelemetry()
                    r.receiver.snapshotTelemetry(self.telemetryInterval)
                # Differentiate the remote device telemetry counters as well
                # (based on their last known value)
                for d in self.devices.values(): d.device.snapshotTelemetry(self.telemetryInterval)
        
    
    # Get a remote device driver object by the device's hardware unique ID
    def getDevice(self, id):
        if not id in self.devices: return None
        return self.devices[id].device
        
        
    # Request a remote device to be dropped and re-discovered (e.g. after rebooting it)
    def dropDevice(self, id):
        if not id in self.devices: return
        self.devices[id].device.destroy()
        with self.rfLock:
            self.devices[id].releaseAddr()
            del self.devices[id]
        
        
    # Attempt to find a route to a device (the receiver that it's connected to and its NodeId)
    def getRoute(self, device):
        with self.rfLock:
            d = self.devices[device.id]
            if d.addr is not None:
                a = d.addr
                if not a.expired(): return d.addr.receiver.receiver, a.nodeid
        raise Exception("No route to device %08X" % device.id.serial)
    
    
    # Send a packet to a remote device (raises an exception if no route is found)
    def sendPacket(self, device, data):
        r, nodeid = self.getRoute(device)
        return r.sendRFPacket(nodeid, data)

    
    # Schedule a remote device to be polled for packets soon
    def pollDevice(self, device):
        r, nodeid = self.getRoute(device)
        r.pollDevice(nodeid)
