# Example script for automated measurements (adapt this to your requirements)
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


import sys
import uuid
import json
import time
import struct
import datetime
import sensorplatform.rfmanager
import sensorplatform.rfdevice
import sensorplatform.receiver

# Measurement attributes
seriesName = "Testmessung"
seriesUUID = uuid.uuid1()
prepareTime = 3000  # milliseconds
# List of devices to wait for. If other devices appear, they will be used as well.
devices = [2, 3, 4, 5, 6, 7, 8, 9]

# Measurement data handler that dumps the measurements to the console as JSON
def handleData(device, sensor, timestamp, data):
    d = sensor.decoder
    print(json.dumps({"device": device.id.idstr, "sensor": sensor.id, "time": timestamp, "component": d.component, "unit": d.unit, "value": data}))

# Create a radio routing hub
manager = sensorplatform.rfmanager.RFManager()
# Find the first receiver device and attach it to the routing hub
receiver = sensorplatform.receiver.Receiver()
manager.addReceiver(receiver)
# Start radio communication with the specified parameters (adapt to your needs)
receiver.startRadio(70, 0, 0, 0, 32, 0, 0)

# Collect devices that participate in the measurement
measuring = []
while devices:
    time.sleep(1)
    print("Waiting for devices:", devices, file=sys.stderr)
    for serial in devices:
        # Try to find a still missing device
        id = sensorplatform.rfdevice.RFDevID(struct.pack("<III", 0x53414149, 0x534d5053, serial))
        if not id in manager.devices: continue
        dev = manager.getDevice(id)
        if not dev.sensorsDiscovered: continue
        # Device has appeared and is initialized, remove it from the list of missing devices
        devices.remove(id.serial)
        # Install measurement data handler
        dev.decodedDataHook = handleData
        # Write series header information
        dev.check(dev.writeSeriesHeaderPage(1, b"\0" * 12 + seriesUUID.bytes_le))
        dev.check(dev.writeSeriesHeaderPage(2, seriesName.encode("utf-8")[:28]))
        # Append it to the list of participating devices
        measuring.append(dev)

# Calculate measurement start time
print("Starting measurement in %f seconds..." % (prepareTime / 1000.), file=sys.stderr)
globalTime = (struct.unpack("<I", receiver.getRadioStats()[4][:4])[0] + prepareTime * 1000) & 0xfffffff
unixTime = int((datetime.datetime.utcnow() - datetime.datetime(1970, 1, 1)).total_seconds() * 1000) + prepareTime

# Initiate measurement on all participating devices
for dev in measuring: dev.check(dev.startMeasurement(1, globalTime, unixTime))

# Wait here until the measurement shall be stopped
time.sleep(20)

# Request the measurement to be stopped on all devices
print("Stopping measurement...", file=sys.stderr)
for dev in measuring: dev.check(dev.stopMeasurement())
# Wait for data buffers to drain and report statistics
for dev in measuring:
    endTime, endOffset, total, lost, txOverflowLost, sdOverflowLost = dev.endMeasurement()
    print("Device %08X: Captured %d bytes, lost out of %d packets: RX=%d TX=%d" % (dev.id.serial, endOffset, total, lost, txOverflowLost), file=sys.stderr)

# Shut down radio communication to put the sensor nodes back into sleep mode
receiver.stopRadio()
