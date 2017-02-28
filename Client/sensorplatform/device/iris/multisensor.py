# IRIS 3D Printer MultiSensor node device driver
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


import sensorplatform.rfdevice
import sensorplatform.device.iris.sensor.mapper
import sys
import time
import math
import bisect
import struct
import binascii
import threading
import traceback
import collections



# CRC32 calculator for firmware upgrade checksumming
def crc32(data):
    result = 0xffffffff
    data = struct.unpack("<%dI" % (len(data) / 4), data)
    for word in data:
        for bit in range(32):
            result = ((result << 1) ^ (0x04c11db7 * ((result ^ word) >> 31))) & 0xffffffff
            word = (word << 1) & 0xffffffff
    return result



class MultiSensorDevice(sensorplatform.rfdevice.RFDevice):

    # Constructor: Initialize object state
    def __init__(self, manager, id):
        self.sensors = {}  # Discovered sensors
        self.seriesHeader = [None] * 16  # Non-sensor-configuration pages of the series header
        self.sensorDataCache = [[None] * 4 for i in range(64)]  # Sensor configuration page cache
        self.sensorDataDirty = [[False] * 4 for i in range(64)]  # Sensor configuration page dirty flags
        self.sensorsDiscovered = False  # Sensor discovery complete flag
        self.decoderActive = False  # Measurement data processing in progress flag
        self.decoderLock = threading.RLock()  # Measurement data processing state lock
        self.decoderTime = 0  # Current data decoding timestamp (microseconds since unix epoch)
        self.decoderSeq = 0  # Current data decoding sequence number
        self.decoderData = b""  # Consecutive data pending to be decoded
        self.decoderBuffer = {}  # Out-of-order data packets pending to be decoded
        self.decoderSchedule = None  # Sensor sampling schedule timestamp list
        self.decoderSensor = None  # Sensor sampling schedule sensor object list
        self.decoderLastProgress = 0  # (Local) time at which the decoder last made progress
        self.decoderLastSkipSeq = 0  # Packet sequence number that the decoder skipped to last
        self.decoderEndTime = None  # Total measurement time reported by device at end of measurement (microseconds)
        self.decoderEndOffset = 0  # Total measurement data size reported by device at end of measurement (bytes)
        self.txOverflowLost = 0  # Data packets lost on radio link reported by device at end of measurement
        self.sdOverflowLost = 0  # Data packets lost on SD card reported by device at end of measurement
        self.lostPackets = 0  # Data packets considered lost (and skipped) during decoding
        self.measurementEndLastNoData = 0  # Last no data info time at the time when a measurement stop was requested
        self.rawDataHook = None  # Raw measurement data stream packet hook
        self.attrDataHook = None  # Sensor configuration/attribute hook
        self.decodedDataHook = None  # Decoded sensor measurement value hook
        # Initialize base class
        sensorplatform.rfdevice.RFDevice.__init__(self, manager, id)
        # Start sensor discovery thread
        threading.Thread(daemon=True, target=self.discoveryThread).start()
        
        
    # Device discovery thread, started on instantiation of a new device
    def discoveryThread(self):
        try:
            # Attempt to leave upload mode just in case, will report success if in idle state
            self.stopUpload()
            # Attempt to read a series header page. If we get a BUSY error, we might be running
            # a measurement, so attempt to stop it (will report success if none is running). All
            # other possible causes for a BUSY error are transient and will clear by themselves.
            while self.readSeriesHeaderPage(0)[0] == 5: self.check(self.stopMeasurement())
            # Download sensor information pages
            self.reloadSensorData()
            # Instantiate sensor drivers for all present sensors
            for i in range(64):
                if self.sensorDataCache[i][0][:12] != b"\0" * 12:
                    self.sensors[i] = sensorplatform.device.iris.sensor.mapper.instantiate(self, i)
            # Flag the sensor node as fully discovered and print sensor list if requested
            self.sensorsDiscovered = True
            if self.manager.printFullyDiscovered: self.printSensorList()
        except Exception:
            # Something went wrong while discovering the device (usually loss of radio link).
            # Drop the device and attempt to re-discover it if it gets detected again.
            traceback.print_exc()
            self.manager.dropDevice(self.id)
        
        
    # (Re-)load sensor information pages
    def reloadSensorData(self):
        # Commit any local changes to the pages before flushing them
        if self.sensorsDiscovered: self.commitAllSensorAttrs()
        # Insert all pages that we need to load (page 0 of every sensor) into the loading queue
        pagequeue = collections.deque([(i, 0) for i in range(64)])
        # Initialize a queue of currently being loaded pages (transfer in progress)
        running = collections.deque()
        # While there's still something to do:
        while len(pagequeue) > 0 or len(running) > 0:
            while len(pagequeue) > 0 and len(running) < 16:
                # Less than 16 transfers are active, so start a new one from the queue.
                sensor, page = pagequeue.popleft()
                seq = self.asyncCmd(0x0104, (sensor << 2) | page)
                # cmdAttempt will be called below while iterating over the running requests
                running.append((sensor, page, seq))
            for i in range(len(running)):
                # Check all running transfers for completion
                req = running.popleft()
                if self.isCmdDone(req[2]):
                    # A transfer has finished. Check the result.
                    status, data = self.finishCmd(req[2])
                    if status != 0:
                        # The page download request was rejected for some reason, retry it later.
                        print("Failed to get sensor %d page %d on device %08X: Device returned status %02X" % (req[0], req[1], self.id.serial, status))
                        pagequeue.append(req[:2])
                        continue
                    # Insert the received page data into the cache
                    self.sensorDataCache[req[0]][req[1]] = data
                    self.sensorDataDirty[req[0]][req[1]] = False
                    if req[1] == 0 and data[:12] != b"\0" * 12:
                        # If this was page 0 of a sensor and the sensor is present,
                        # request the other information pages of it as well.
                        pagequeue.extend([(req[0], i) for i in range(1, 4)])
                else:
                    # The transfer hasn't finished yet. Retransmit the request.
                    self.cmdAttempt(req[2])
                    running.append(req)
            # If any requests are in progress, wait for 10ms before
            # checking status or attempting to spawn new ones again.
            if len(running) > 0: time.sleep(0.01)
    
    
    # Print the list of sensors present on the sensor node
    def printSensorList(self):
        if not self.sensorsDiscovered: print("Device %08X sensors aren't fully discovered yet.")
        else:
            print("Device %08X sensors:" % self.id.serial)
            for id in self.sensors:
                print("    Sensor %2d: %s" % (id, self.sensors[id].name))

                
    # Write back all locally changed sensor information pages to the sensor node
    def commitAllSensorAttrs(self):
        pagequeue = collections.deque()  # Queue of pages needing to be written back
        # Insert all pages that have the dirty flag set into the queue
        for sensor in range(64):
            for page in range(4):
                if self.sensorDataDirty[sensor][page]:
                    pagequeue.append((sensor, page))
        running = collections.deque()  # Queue of page writes currently in progress
        errors = ""  # Error summary string of all transfers
        # While there's still something to do:
        while len(pagequeue) > 0 or len(running) > 0:
            while len(pagequeue) > 0 and len(running) < 16:
                # Less than 16 transfers are active, so start a new one from the queue.
                sensor, page = pagequeue.popleft()
                seq = self.asyncCmd(0x0105, (sensor << 2) | page, self.sensorDataCache[sensor][page])
                # cmdAttempt will be called below while iterating over the running requests
                running.append((sensor, page, seq))
            for i in range(len(running)):
                # Check all running transfers for completion
                req = running.popleft()
                if self.isCmdDone(req[2]):
                    # A transfer has finished. Check the result.
                    result, data = self.finishCmd(req[2])
                    if result != 0:
                        # The page write request was rejected for some reason, report an error.
                        errors += "\nCommitting sensor %d page %d failed: Device returned status %02X" % (req[0], req[1], result)
                    else:
                        # The page write request succeeded, so insert the actually applied sensor
                        # configuration (as reported back) into the cache and clear the dirty flag.
                        self.sensorDataCache[req[0]][req[1]] = data
                        self.sensorDataDirty[req[0]][req[1]] = False
                else:
                    # The transfer hasn't finished yet. Retransmit the request.
                    self.cmdAttempt(req[2])
                    running.append(req)
            # If any requests are in progress, wait for 10ms before
            # checking status or attempting to spawn new ones again.
            if len(running) > 0: time.sleep(0.01)
        # If we hit any errors while writing sensor configuration,
        # throw an exception containing a list of them.
        if errors != "": raise Exception("Errors while committing sensor attributes to device %08X: %s" % (self.id.serial, errors))

        
    # (Synchronously) read a single series header page
    def readSeriesHeaderPage(self, page):
        return self.cmd(0x0102, page)

        
    # (Synchronously) write a single series header page
    def writeSeriesHeaderPage(self, page, data):
        return self.cmd(0x0103, page, data)

        
    # (Synchronously) save the series header to the SD card
    def saveSeriesHeader(self):
        self.commitAllSensorAttrs()
        return self.cmd(0x0107, 0)

        
    # (Synchronously) enter firmware upload mode
    def startUpload(self):
        return self.cmd(0x01f0, 0)

        
    # (Synchronously) leave firmware upload mode (without actually upgrading)
    def stopUpload(self):
        return self.cmd(0x01f1, 0)

        
    # Upload sector buffer contents (in firmware upload mode)
    def uploadBuffer(self, subject, data):
        pagequeue = collections.deque()  # Queue of data blocks needing to be written
        # Slice the sector buffer into pages and put them into the queue
        for page in range(math.ceil(len(data) / 28.)):
            pagequeue.append((page, data[page * 28 : (page + 1) * 28]))
        running = collections.deque()  # Queue of page writes currently in progress
        errors = ""  # Error summary string of all transfers
        # While there's still something to do:
        while len(pagequeue) > 0 or len(running) > 0:
            while len(pagequeue) > 0 and len(running) < 16:
                # Less than 16 transfers are active, so start a new one from the queue.
                page, data = pagequeue.popleft()
                seq = self.asyncCmd(0x01f2, page, data)
                # cmdAttempt will be called below while iterating over the running requests
                running.append((page, seq))
            for i in range(len(running)):
                # Check all running transfers for completion
                req = running.popleft()
                if self.isCmdDone(req[1]):
                    # A transfer has finished. Check the result.
                    result, data = self.finishCmd(req[1])
                    # If the transfer was rejected for some reason, report an error.
                    if result != 0: errors += "\nUpload page %d failed: Device returned status %02X" % (req[0], result)
                else:
                    # The transfer hasn't finished yet. Retransmit the request.
                    self.cmdAttempt(req[1])
                    running.append(req)
            # If any requests are in progress, wait for 10ms before
            # checking status or attempting to spawn new ones again.
            if len(running) > 0: time.sleep(0.01)
        # If we hit any errors while writing sensor configuration,
        # throw an exception containing a list of them.
        if errors != "": raise Exception("Errors while uploading %s: %s" % (subject, errors))

        
    # Upload data to an SD card sector (in firmware upload mode)
    def uploadSector(self, sector, data):
        # Upload the data to the sector buffer
        self.uploadBuffer("sector %d" % sector, data)
        # Write the sector buffer to the SD card
        self.check(self.cmd(0x01f3, 0, struct.pack("<I", sector)))

        
    # Upgrade the sensor node's firmware (asynchronously)
    def upgradeFirmware(self, updater, image, status=None):
        # Upgrade function, called asynchronously below.
        def doUpgrade():
            # Ensure that we have access to the arguments of the outer function
            nonlocal self, updater, image, status
            # Enter firmware upload mode
            self.check(self.startUpload())
            # Calculate the number of sectors of the firmware image
            size = math.ceil(len(image) / 512.)
            # Pad the image to the next sector boundary with zero bytes
            image = image.ljust(size * 512, b"\0")
            # Calculate the CRC32 checksum of the firmware image
            crc = crc32(image)
            # Upload the sectors of the firmware image to the SD card and
            # call the progress callback after every completed sector.
            for i in range(size):
                self.uploadSector(i, image[i * 512 : (i + 1) * 512])
                if status is not None: status(self, i / (size + 1.))
            # Upload the updater code to the sector buffer (in RAM)
            self.uploadBuffer("updater", updater)
            # Initiate firmware upgrade, pass control to the updater code
            self.check(self.cmd(0x01f4, 0, struct.pack("<II", size, crc)))
            # Report success to the progress callback
            if status is not None: status(self, 1)
            # Drop this device driver instance as the device will
            # need to be re-discovered after the upgrade.
            self.manager.dropDevice(self.id)
        # Spawn a thread that performs the upgrade
        threading.Thread(daemon=True, target=doUpgrade).start()

        
    # Immediately reboot the sensor node
    def reboot(self):
        # Send reboot request (TODO: This will always fail due to lack of response)
        self.cmd(0x01ff, 0)
        # Drop the device driver instance and attempt to re-discover the device
        self.manager.dropDevice(self.id)

        
    # Initiate a measurement at the specified time
    def startMeasurement(self, targets, globalTime, unixTime):
        # Apply any pending sensor configuration changes
        self.commitAllSensorAttrs()
        # Initialize decoder state:
        with self.decoderLock:
            self.seriesHeader = [None] * 16
            self.decoderTime = int(unixTime * 1000)
            self.decoderSeq = 0
            self.decoderData = b""
            self.decoderBuffer = {}
            self.decoderSchedule = collections.deque()
            self.decoderSensor = collections.deque()
            self.decoderLastProgress = time.monotonic() + 3  # Wait up to 5 seconds for header data
            self.decoderLastSkipSeq = 0
            self.decoderActive = True
            self.decoderEndTime = None
            self.decoderEndOffset = 0xffffffffffffffff
            self.lostPackets = 0
            self.measurementEndLastNoData = 0
        # Start the measurement on the sensor node
        return self.cmd(0x0110, targets, struct.pack("<IQ", globalTime, unixTime))


    # Terminate a measurement
    def stopMeasurement(self):
        # Send measurement stop request to the sensor node
        status, data = self.cmd(0x0111, 0)
        # If the measurement wasn't started by us, just return success without result data.
        if not self.decoderActive: return 0, None
        # Capture the last time that the sensor node's buffers were empty.
        # Once that changes we know that all data of the measurement has been transferred.
        self.measurementEndLastNoData = self.lastNoData
        # Store measurement completion information passed by the sensor node for endMeasurement()
        self.decoderEndTime, self.decoderEndOffset, self.txOverflowLost, self.sdOverflowLost = struct.unpack("<IQII", data[:20])
        # Return the result of the stop operation
        return status, data
        

    # Wait for processing of a stopped measurement to finish and report completion information
    def endMeasurement(self):
        # If we aren't aware of an unfinished measurement, just return.
        if not self.decoderActive: return
        # Wait for the sensor node's data buffer to become empty.
        while self.lastNoData == self.measurementEndLastNoData: time.sleep(0.01)
        # Mark the measurement as finished.
        self.decoderActive = False
        # Decode any remaining data packets. If any data is still missing, it will never arrive.
        with self.decoderLock:
            while self.decoderSeq * 28 < self.decoderEndOffset:
                self.decodePacket(self.decoderBuffer.pop(self.decoderSeq, b"\0" * 28))
        # Report measurement completion information:
        #     decoderEndTime: Measurement duration in microseconds (will wrap after exceeding 32 bits)
        #     decoderEndOffset: Measurement data size in bytes
        #     decoderSeq: Number of data packets in the measurement
        #     lostPackets: Packets skipped by the decoder because they didn't arrive in time
        #     txOverflowLost: Packets never transmitted by the sensor node due to buffer overflows
        #     sdOverflowLost: Packets not written to the SD card due to it not being able to keep up
        return self.decoderEndTime, self.decoderEndOffset, self.decoderSeq, self.lostPackets, self.txOverflowLost, self.sdOverflowLost

        
    # Process an incoming measurement data packet (which is possibly out of sequence)
    def handleDataPacket(self, frame, seq, data):
        # Kill anything that tries to communicate with lost/disconnected devices
        if self.drop: return
        #print("%.6f" % time.monotonic(), "got", self.id.serial, seq, self.decoderSeq)
        # Pass the raw received packet to the hook if there is one
        if self.rawDataHook is not None: self.rawDataHook(self, frame, seq, data)
        with self.decoderLock:
            # If we aren't aware of an unfinished measurement, ignore the packet.
            if not self.decoderActive: return
            # If we have been stuck waiting for a missing packet for more than 2 seconds,
            # it will probably never arrive. Move on and try to catch up again.
            now = time.monotonic()
            skip = now - self.decoderLastProgress > 2
            # If the packet is in sequence, just process it.
            if seq == self.decoderSeq: self.decodePacket(data)
            # If packets are missing in between, just enqueue the just
            # received one and wait for the missing ones to arrive first.
            elif seq > self.decoderSeq: self.decoderBuffer[seq] = data
            # TODO: Warn about late arriving (and already skipped) packets here
            # Try to process packets from the buffer (we might have just filled a gap)
            while True:
                if not self.decoderSeq in self.decoderBuffer:
                    # The next required packet isn't in the buffer. If:
                    # - we were already decoding ahead of the just received packet
                    # - or we haven't waited for at least 2 seconds since the last progress
                    # - or we would move past the point in the data stream that we skipped to
                    # stop and wait for any delayed packets to arrive.
                    if seq < self.decoderSeq or not skip or self.decoderSeq >= self.decoderLastSkipSeq: break
                    else:
                        # If none of the three conditions were met, consider the packet lost.
                        print("%.6f" % time.monotonic(), "LOST", self.id.serial, self.decoderSeq, seq)
                        if self.decoderSeq < 272:
                            # If it was a header packet, this might confuse the decoder. Warn about that.
                            print("WARNING! Lost series header packet %d for device %08X, decoded data may be garbage!" % (self.decoderSeq, self.id.serial))
                        # Insert zero bytes and increment the lost packet counter
                        self.decodePacket(b"\0" * 28)
                        self.lostPackets += 1
                # If the next required packet is in the buffer, process it
                else: self.decodePacket(self.decoderBuffer.pop(self.decoderSeq))
                # Loop until we hit one of the three cancellation conditions above
            # If the decoder is in sync with the just received packet, consider that progress.
            if self.decoderSeq == seq + 1: self.decoderLastProgress = time.monotonic()
            if skip:
                # If we have skipped packets, consider that progress as well.
                # Prevent the next skip from going past the just received packet,
                # so that we never skip anything that isn't at least 2 seconds late.
                self.decoderLastSkipSeq = seq
                self.decoderLastProgress = now
        
        
    # Decode a single data packet (called with packets in sequence and gaps filled with zeros)
    def decodePacket(self, data):
        # If we would go past the end of the measurement (into the padding at the end), stop here.
        offset = self.decoderSeq * 28
        if offset >= self.decoderEndOffset: return
        # If this is a series header packet, just store it.
        if self.decoderSeq < 16: self.seriesHeader[self.decoderSeq] = data
        elif self.decoderSeq < 272:
            # This is a sensor configuration packet, just store it.
            self.sensorDataCache[(self.decoderSeq - 16) >> 2][self.decoderSeq & 3] = data
            if self.decoderSeq == 271:
                # This was the last header packet. For each sensor:
                for s in self.sensors.values():
                    # Initialize its decoder (apply configuration)
                    s.decoder.update()
                    if s.decoder.interval > 0 and s.decoder.recordBytes > 0:
                        # The sensor is active. Insert it into the schedule.
                        self.scheduleSensor(s, s.decoder.offset)
                        if self.attrDataHook is not None:
                            # There is a sensor attribute hook. Emit all attributes of the sensor.
                            for attr in s.attrs.keys():
                                self.attrDataHook(self, s, attr, s.getAttr(attr))
        # This is a sensor measurement packet.
        # If there are any active sensors, attempt to decode it.
        elif len(self.decoderSensor) > 0:
            # Insert the packet data into the decoder buffer
            self.decoderData += data[:self.decoderEndOffset - offset]
            # While there is enough data in the decoder buffer to decode the next data point:
            while len(self.decoderData) >= self.decoderSensor[0].decoder.recordBytes:
                # Grab the next sensor in the schedule,
                self.decoderTime = self.decoderSchedule.popleft()
                sensor = self.decoderSensor.popleft()
                # let its decoder decode the data,
                sample = sensor.decoder.decode(self.decoderData[:sensor.decoder.recordBytes])
                # pass the decoded data to the decoded data hook if present,
                if self.decodedDataHook is not None: self.decodedDataHook(self, sensor, self.decoderTime / 1000., sample)
                # remove the raw data from the decoder buffer
                self.decoderData = self.decoderData[sensor.decoder.recordBytes:]
                # and re-schedule the sensor for its next measurement time.
                self.scheduleSensor(sensor)
        # Increment decoded packet counter
        self.decoderSeq += 1
        
        
    # Insert a sensor into the decoder's measurement schedule.
    # This needs to exactly match the corresponding behavior on the sensor node side
    # in order to yield the sensors in the right order for decoding the recorded data.
    def scheduleSensor(self, sensor, interval=None):
        # Move the sensor forward by its measurement interval (or the specified interval)
        if interval is None: interval = sensor.decoder.interval
        time = self.decoderTime + interval
        # Figure out at which position in the queue it needs to be inserted
        index = bisect.bisect(self.decoderSchedule, time)
        # Insert the sensor (and its next sampling time) into the queue
        self.decoderSchedule.insert(index, time)
        self.decoderSensor.insert(index, sensor)
