# Command line interface and script processor for simple measurement
# operations using IRIS 3D Printer MultiSensor node devices
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


import sensorplatform.rfmanager
import sensorplatform.rfdevice
import sensorplatform.receiver
import sys
import os
import cmd
import time
import json
import uuid
import queue
import shlex
import struct
import binascii
import datetime
import threading
import traceback

VERSION = "0.0.1"


def parseBool(s): return s.strip().lower() in ["1", "true", "t", "yes", "y", "on"]

# Check the result of a USB command. Throw an exception if it was not successful.
def checkStatus(msg, seq, reserved, status, payload):
    if msg == 0x8001 and status == 0: return  # Success response
    raise Exception("Command failed with status %04X:%08X" % (msg, status))


# Main command line processor class
class Client(cmd.Cmd):

    # Constructur: Initialize state and configure base class
    def __init__(self, manager, receiver):
        cmd.Cmd.__init__(self)  # Call base class constructor
        self.printDiscovered = False  # Whether to print messages about newly discovered devices
        self.prompt = "> "  # Input prompt string
        self.exit = False  # Whether the processed command wants to terminate the (sub-)shell
        self.manager = manager  # Radio routing hub
        self.receiver = receiver  # Radio receiver device
        self.measuring = None  # List of devices that are currently measuring
        self.seriesUUID = None  # Series UUID of the currently running measurement
        self.dataBuffer = queue.Queue()  # Outgoing measurement data message buffer
        self.submitInterval = 10  # Interval (in seconds) how often to submit dataBuffer
        self.submitUrl = None  # URL to submit dataBuffer to
        # Start measurement data sender thread
        threading.Thread(daemon=True, target=self.submitThread).start()

    # Ignore empty commands
    def emptyline(self): pass
    
    # Wrapper to catch exceptions and drop back to the next interactive shell
    def onecmd(self, str):
        try:
            return super(Client, self).onecmd(str)
        except Exception:
            traceback.print_exc()
            return True
    
    def do_setsubmiturl(self, arg):
        "Configure the URL that measurement data should be submitted to."
        self.submitUrl = arg
    
    def do_setsubmitinterval(self, arg):
        "Configure the interval (in seconds) how often measurement data should be submitted."
        self.submitInterval = float(arg)
    
    def do_printusbpackets(self, arg):
        "Configure whether to print USB packets or not."
        self.receiver.printUSBPackets = parseBool(arg)
    
    def do_printrfpackets(self, arg):
        "Configure whether to print radio packets or not."
        self.receiver.printRFPackets = parseBool(arg)
    
    def do_printdroppedpackets(self, arg):
        "Configure whether to print dropped radio packets or not."
        self.manager.printDroppedPackets = parseBool(arg)
    
    def do_printdevdiscovered(self, arg):
        "Configure whether to print a message if a new device is discovered."
        self.printDiscovered = parseBool(arg)
    
    def do_printsensorsdiscovered(self, arg):
        "Configure whether to list the sensors of a device once they have been discovered."
        self.manager.printFullyDiscovered = parseBool(arg)
    
    def do_stopradio(self, arg):
        "Stops radio communcation."
        checkStatus(*self.receiver.stopRadio())
    
    def do_startradio(self, arg):
        ("startradio <channel> <speed> <txPower> <receiverTxPower> <guardBits> <preGapBits> <postGapBits> <netId>\n"
         "Configures the radio receiver and starts up communication.\n"
         "    <channel>         - Center frequency is 2400 + <channel> MHz\n"
         "    <speed>           - Air data rate: 0: 2Mbit/s, 1: 1Mbit/s, 3: 250kbit/s, default 0\n"
         "    <txPower>         - Node TX power: 0: -18dBm, 1: -12dBm, 2: -6dBm, 0: 0dBm, default 0\n"
         "    <receiverTxPower> - Node TX power: 0: -18dBm, 1: -12dBm, 2: -6dBm, 0: 0dBm, default 0\n"
         "    <guardBits>       - Number of guard bits between time slots, default 16\n"
         "    <preGapBits>      - Number of additional gap bits before the first RX time slot, default 0\n"
         "    <postGapBits>     - Number of additional gap bits after the last RX time slot, default 0\n"
         "    <netId>           - Network session identifier (0-255), default is random\n")
        args = list(map(int, shlex.split(arg)))
        if len(args) == 0: raise Exception("No channel frequency specified.")
        checkStatus(*self.receiver.startRadio(*args))
    
    def do_sendpacket(self, arg):
        "sendpacket <nodeId> <hexdata>"
        arg = arg.strip().split(" ", 2)
        nodeId = int(arg[0], 0)
        packet = binascii.unhexlify(arg[1].encode("ascii"))
        checkStatus(*self.receiver.sendRFPacket(nodeId, packet))
        
    def do_listnodes(self, arg):
        "Lists all currently connected and supported RF devices."
        print("Connected devices:")
        for id in manager.devices.keys():
            # Ignore devices that haven't been identified yet
            if id.info is None: continue
            # Ignore devices that don't implement the SensorPlatform protocol
            if id.info.protoVendor != 0x53414149 or id.info.protoType != 0x5053 or id.info.protoVersion != 0: continue
            # Ignore devices that aren't IRIS 3D Printer MultiSensor nodes
            if id.vendor != 0x53414149 or id.product != 0x534d5053: continue
            # Print information about the device
            d = self.manager.getDevice(id)
            print("  Serial number %08X: firmware version %04X, %d sensors" % (id.serial, id.info.fwVersion, len(d.sensors)))
    
    def do_listsensors(self, arg):
        ("listsensors <serialhex>\n"
         "Lists all sensor channels present on the specified device.")
        d = self.getDevice(arg)
        if d is not None: d.printSensorList()
    
    def do_listsensorattrs(self, arg):
        ("listsensorattrs <serialhex> <sensorid>\n"
         "Lists all attributes of the specified sensor on the specified device.")
        arg = shlex.split(arg)
        d, s = self.getSensor(arg[0], arg[1])
        if s is not None:
            print("Device %08X sensor %d attributes:" % (d.id.serial, s.id))
            for name, attr in s.attrs.items():
                print(("    %s: " + attr.printAs) % (name, s.getAttr(name)))
    
    def do_getsensorattr(self, arg):
        ("getsensoraddr <serialhex> <sensorid> <attrname>\n"
         "Shows the specified attribute of the specified sensor on the specified device.")
        arg = shlex.split(arg)
        d, s, a = self.getSensorAttr(arg[0], arg[1], arg[2])
        if a is not None: print(a.printAs % s.getAttr(arg[2]))
    
    def do_setsensorattr(self, arg):
        ("setsensorattr <serialhex> <sensorid> <attrname> <value>\n"
         "Shows the specified attribute of the specified sensor on the specified device.")
        arg = shlex.split(arg)
        d, s, a = self.getSensorAttr(arg[0], arg[1], arg[2])
        if a is not None: s.setAttr(arg[2], arg[3])
    
    def do_syncsensorattrs(self, arg):
        ("syncsensorattrs <serialhex>\n"
         "Synchronizes sensor attributes with the specified device.")
        d = self.getDevice(arg)
        if d is not None: d.reloadSensorData()
    
    def do_saveseriesheader(self, arg):
        ("saveseriesheader <serialhex>\n"
         "Saves series header and sensor attributes to non-volatile storage.")
        d = self.getDevice(arg)
        if d is not None: d.check(d.saveSeriesHeader())
        
    def do_startmeasurement(self, arg):
        ("startmeasurement <namestr> <preparetime> <liveTx> <recordSD> <serialhex...>\n"
         "Starts a measurement with the specified name on the specified list of boards.\n"
         "namestr may be up to 28 bytes long in UTF-8 encoding.\n"
         "preparetime is the time in milliseconds that is used to synchronize and prepare devices.")
        arg = shlex.split(arg)
        # Consume first argument: Measurement name (convert to UTF-8, truncate to 28 bytes)
        name = arg.pop(0).encode("utf-8")[:28]
        # Consume second argument: Time in milliseconds until measurement should begin
        prepareTime = int(arg.pop(0))
        # Fill bit field which targets (radio, SD card) to record to
        targets = 0
        if parseBool(arg.pop(0)): targets |= 1
        if parseBool(arg.pop(0)): targets |= 2
        # Fail if there's already a measurement in progress
        if self.measuring is not None:
            print("There is already a measurement in progress.")
            return
        # Flush out any remaining data from previous measurements
        self.submitData()
        # Generate a new (random) measurement series UUID
        self.seriesUUID = uuid.uuid1()
        # Find all specified devices that should be part of the measurement
        devices = []
        for serialhex in arg:
            d = self.getDevice(serialhex)
            # Fail if a specified device cannot be found (self.getDevice prints an error message)
            if d is None: return
            # Apply any pending sensor configuration changes and write series header data
            d.commitAllSensorAttrs()
            d.check(d.writeSeriesHeaderPage(1, b"\0" * 12 + self.seriesUUID.bytes_le))
            d.check(d.writeSeriesHeaderPage(2, name))
            # The startMeasurement command below will save the series header to persistent storage
            devices.append(d)
        # Keep track of devices participating in the measurement
        self.measuring = devices
        # Calculate start time of the measurement as unix time and from the base station's perspective
        globalTime = (struct.unpack("<I", self.receiver.getRadioStats()[4][:4])[0] + prepareTime * 1000) & 0xfffffff
        unixTime = int((datetime.datetime.utcnow() - datetime.datetime(1970, 1, 1)).total_seconds() * 1000) + prepareTime
        # Initiate measurement on all participating sensor nodes
        for d in self.measuring: d.check(d.startMeasurement(targets, globalTime, unixTime))
    
    def do_stopmeasurement(self, arg):
        "Stops the currently running measurement."
        if self.measuring is None:
            print("There is currently no measurement in progress.")
            return
        # Stop measurement on all participating sensor nodes
        for d in self.measuring: d.check(d.stopMeasurement())
        # Wait for data processing to finish on all participating sensor nodes and report statistics
        for d in self.measuring:
            endTime, endOffset, total, lost, txOverflowLost, sdOverflowLost = d.endMeasurement()
            print("Device %08X: Captured %d bytes, lost out of %d packets: RX=%d TX=%d SD=%d" % (d.id.serial, endOffset, total, lost, txOverflowLost, sdOverflowLost))
        self.measuring = None
    
    def do_rediscover(self, arg):
        ("rediscover [<serialhex...> | all]\n"
         "Rediscovers the specified devices.")
        args = shlex.split(arg)
        if args == ["all"]: devices = [d.device for d in manager.devices.values()]
        else:
            devices = []
            for serialhex in args:
                d = self.getDevice(serialhex)
                if d is None: return
                devices.append(d)
        for d in devices: self.manager.dropDevice(d.id)
    
    def do_reboot(self, arg):
        ("reboot [<serialhex...> | all]\n"
         "Reboots the specified devices.")
        args = shlex.split(arg)
        if args == ["all"]: devices = [d.device for d in manager.devices.values()]
        else:
            devices = []
            for serialhex in args:
                d = self.getDevice(serialhex)
                if d is None: return
                devices.append(d)
        for d in devices: d.reboot()
    
    def do_upgradefirmware(self, arg):
        ("upgradefirmware <updater> <image> [<serialhex...> | all]\n"
         "Updates the firmware of the specified devices.")
        args = shlex.split(arg)
        # Read updater and firmware image
        with open(args.pop(0), "rb") as f: updater = f.read()
        with open(args.pop(0), "rb") as f: image = f.read()
        # Build the list of devices to upgrade
        if args == ["all"]: devices = [d.device for d in manager.devices.values()]
        else:
            devices = []
            for serialhex in args:
                d = self.getDevice(serialhex)
                if d is None: return
                devices.append(d)
        # Initialize map to keep track of progress on each device
        status = {d.id: 0 for d in devices}
        # Progress change callback, called by the device driver
        def statusCallback(d, progress):
            # Update progress information of the device
            status[d.id] = progress
            # Print updated progress information
            sys.stdout.write("Upgrading:")
            for id, progress in status.items(): sys.stdout.write(" %X:%3d%%" % (id.serial, progress * 100))
            sys.stdout.write("\r")
            sys.stdout.flush()
        # Initiate firmware upgrade on the devices to be upgraded (runs asynchronously)
        for d in devices:
            d.upgradeFirmware(updater, image, statusCallback)
        # Wait for the upgrade to finish on all devices before returning
        while True:
            for progress in status.values():
                if progress != 1: break
            else:
                print()
                return
            time.sleep(0.1)
    
    def do_setrfstatsinterval(self, arg):
        "Configure the RF statistics summarization interval."
        self.manager.telemetryInterval = float(arg)
    
    def do_rfstats(self, arg):
        "Show radio layer statistics"
        rd = self.receiver.telemetryDelta
        print("Radio layer statistics:")
        print("    Network:")
        if rd is None:
            print("        No data available")
            return
        print("        Frames per second: %10.1f" % rd[0])
        print("        TX packets per second: %6.1f (%9.1fkbit/s)" % (rd[1], rd[1] * 28 * 8))
        print("        RX packets per second: %6.1f (%9.1fkbit/s)" % (rd[2], rd[2] * 28 * 8))
        print("        RX rejects per second: %6.1f" % rd[3])
        print("        RX overflows per second: %4.1f" % rd[4])
        print("    Nodes:")
        for id in self.manager.devices:
            d = self.manager.getDevice(id)
            try: r, nid = self.manager.getRoute(d)
            except: r, nid = None, 0
            dd = d.telemetryDelta
            if nid == 0: info = "Connection lost"
            elif dd is None: info = "No data available"
            elif dd[0] == 0: info = "Outdated information"
            else: info = "Frames: %5.1f%% lost, %5.1f%% discont, %5.1f%% syncerr, TX: %6.1f/s (%5.1f%% lost), RX: %5.1f/s" % \
                         (100 * (1 - dd[1] / dd[0]), 100 * dd[3] / dd[1] if dd[1] else 0, 100 * dd[2] / dd[1] if dd[1] else 0, dd[4], 100 * (1 - dd[5] / dd[4]) if dd[4] else 0, dd[6])
            print("        Serial %08X: %s" % (id.serial, info))
            
    def do_rfthroughputtest(self, arg):
        "rfthroughputtest <serialhex> <seconds>"
        arg = shlex.split(arg)
        d = self.getDevice(arg[0])
        if d is not None:
            # Assign all slots to the specified device and count how many responses we get
            cycles = int(float(arg[1]) / self.manager.telemetryInterval)
            r, addr = self.manager.getRoute(d)
            r.assignSlots([addr | 0x80] * 28)
            try:
                for i in range(cycles):
                    self.do_rfstats("")
                    time.sleep(self.manager.telemetryInterval)
                    print("")
            except KeyboardInterrupt: print("Canceled")
            finally: r.assignSlots([0x00] * 28)
        
    def do_echo(self, arg):
        "Prints its arguments. Useful for output from scripts."
        print(arg)

    def do_sleep(self, arg):
        "Sleeps for the specified number of seconds."
        time.sleep(float(arg))

    def do_script(self, arg):
        "Execute one or more script files passed as arguments. An .spcs file extension will be appended."
        # Iterate over the list of script files passed as arguments
        for filename in shlex.split(arg):
            # Iterate over the lines of each script file
            with open(filename + ".spcs", "r") as file:
                # Build command buffer (may consist of multiple lines)
                buffer = ""
                for line in file:
                    # Remove comments (everything after a "#" character)
                    line = line.split("#", 2)[0].strip()
                    # Join the next line if the current one ends with a "\" character
                    if len(line) > 0 and line[-1] == "\\": buffer += line[:-1]
                    else:
                        # Process the command. If it fails or is an exit command (as indicated
                        # by returning True), drop back to the next interactive shell.
                        if client.onecmd(buffer + line): return True
                        buffer = ""

    def do_shell(self, arg):
        "Spawn a sub shell, useful for interactive debugging in scripts."
        while not self.exit: client.cmdloop()
        self.exit = False

    def do_exit(self, arg):
        "Leave the SensorPlatform Client Shell."
        # Causes every subshell including the next interactive one to terminate
        self.exit = True
        return True
        
    # New device detected (but not yet fully discovered) hook
    def handleNewDevice(self, id):
        # Ignore devices that haven't been identified
        if id.info is None: return
        # Ignore devices that don't implement the SensorPlatform protocol
        if id.info.protoVendor != 0x53414149 or id.info.protoType != 0x5053 or id.info.protoVersion != 0: return
        # Ignore devices that aren't IRIS 3D Printer MultiSensor nodes
        if id.vendor != 0x53414149 or id.product != 0x534d5053: return
        # Install measurement data hooks for the device
        d = self.manager.getDevice(id)
        d.rawDataHook = self.handleRawData
        d.attrDataHook = self.handleAttrData
        d.decodedDataHook = self.handleDecodedData
        # Print new device detected message
        print("New device detected: Serial number %08X, firmware version %04X" % (id.serial, id.info.fwVersion))
        
    # Sensor raw measurement data packet received hook. Just put the message into dataBuffer.
    def handleRawData(self, device, frame, seq, data):
        if self.seriesUUID is None: return
        self.dataBuffer.put({"type": "raw", "device": device.id.idstr, "frame": frame, "seq": seq, "time": time.time() * 1000, "data": binascii.hexlify(data).decode("ascii")})
    
    # Sensor configuration attribute hook. Just put the message into dataBuffer.
    def handleAttrData(self, device, sensor, attr, value):
        if self.seriesUUID is None: return
        self.dataBuffer.put({"type": "attr", "device": device.id.idstr, "sensor": sensor.id, "attr": attr, "value": value})
    
    # Sensor decided measurement data hook. Just put the message into dataBuffer.
    def handleDecodedData(self, device, sensor, timestamp, data):
        if self.seriesUUID is None: return
        d = sensor.decoder
        self.dataBuffer.put({"type": "decoded", "device": device.id.idstr, "sensor": sensor.id, "time": timestamp, "component": d.component, "unit": d.unit, "value": data})
        
    # Convenience function to fetch a MultiSensor device by its serial number
    # and print an error if that device is not present.
    def getDevice(self, serialhex):
        try: serial = int(serialhex, 16)
        except:
            print("Failed to parse serial number.")
            return None
        id = sensorplatform.rfdevice.RFDevID(struct.pack("<III", 0x53414149, 0x534d5053, serial))
        if not id in self.manager.devices:
            print("Cannot find a device with that serial number.")
            return None
        return self.manager.getDevice(id)
        
    # Convenience function to fetch a Sensor within a MultiSensor device by the device serial
    # number and its sensor ID, and print an error if it is not present.
    def getSensor(self, serialhex, sensorid):
        try: id = int(sensorid)
        except:
            print("Failed to parse sensor id.")
            return None, None
        d = self.getDevice(serialhex)
        if d is None: return None, None
        if not id in d.sensors:
            print("The speicifed sensor does not exist on that device.")
            return d, None
        return d, d.sensors[id]
        
    # Convenience function to fetch an attribute of a Sensor within a MultiSensor device by the
    # device serial number, sensor ID and attribute name, and print an error if it is not present.
    def getSensorAttr(self, serialhex, sensorid, attr):
        d, s = self.getSensor(serialhex, sensorid)
        if s is None: return d, s, None
        if not attr in s.attrs:
            print("The speicifed sensor does not have that attribute.")
            return d, s, None
        return d, s, s.attrs[attr]
        
    # Measurement data sender thread. Periodically calls submitData if submitUrl is set.
    def submitThread(self):
        while True:
            time.sleep(self.submitInterval)
            if self.submitUrl is None or self.seriesUUID is None or self.dataBuffer.empty(): continue
            self.submitData()
            
    # Submit all accumulated data from dataBuffer to the specified submitUrl
    def submitData(self):
        # Drain the buffer into a list
        data = []
        while not self.dataBuffer.empty(): data.append(self.dataBuffer.get())
        # If we don't have anywhere to submit to, discard the data and return.
        if self.submitUrl is None or self.seriesUUID is None: return
        # Serialize the data as JSON.
        # TODO: This occupies the python interpreter's global interpreter lock (GIL) for
        # significant amounts of time at once without letting other threads operate, and
        # might thereby cause delays in processing of USB packets, leading to buffer
        # overflows on the radio receiver device. If large amounts of data are to be
        # serialized, set the submitInterval to be very small (e.g. 100ms). We should
        # figure out a way how to do this while releasing the GIL, but it's not easy.
        data = json.dumps({"series": str(self.seriesUUID), "data": data})
        # If the URL starts with "print://", just print the JSON data to the console
        if self.submitUrl.startswith("print://"): print(data)
        # If it starts with "file://", append it to the specified file
        elif self.submitUrl.startswith("file://"):
            with open(self.submitUrl[7:], "a") as f:
                f.write(data + "\n")
        # Other data sinks (e.g. HTTP) could be implemented here.

        
if __name__ == '__main__':
    print("SensorPlatform Client v%s by Michael Sparmann\n" % VERSION)
    # Create a new radio routing hub
    manager = sensorplatform.rfmanager.RFManager()
    # Find the first radio receiver device and attach it to the routing hub
    receiver = sensorplatform.receiver.Receiver()
    manager.addReceiver(receiver)
    # Instantiate a command processor
    client = Client(manager, receiver)
    # Install its new device detected hook into the routing hub
    manager.newDeviceHook = client.handleNewDevice
    
    try:
        # Execute all scripts specified as command line arguments, if present
        if len(sys.argv) > 1: client.onecmd("script " + " ".join(shlex.quote(a) for a in sys.argv[1:]))
        else:
            # Otherwise spawn an interactive shell
            print("\nWelcome to the SensorPlatform Client. Type help or ? to list commands.\n")
            while not client.exit: client.cmdloop()
    # Handle Ctrl+C without python dumping an exception stack trace
    except KeyboardInterrupt: pass
    
    # Upon termination flush measurement data buffer
    if client.submitUrl is not None: client.submitData()
