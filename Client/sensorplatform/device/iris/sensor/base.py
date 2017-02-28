# Sensor driver and decoder base classes
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


import struct



# A class representing a sensor attribute,
# defined as an interpreatation of a bit field in a configuration page.
class Attribute(object):
    def __init__(self, page, offset, format, printAs="%s", mask=0, shift=0, map=None, xlate=None):
        self.page = page  # Number of the page that contains the attribute
        self.offset = offset  # Byte offset within the page
        self.format = format  # struct.pack format of the field
        self.size = struct.calcsize(format)  # Size in bytes of the field
        self.printAs = printAs  # String formatting pattern how to print the attribute
        self.mask = mask  # Bit mask of this field within the struct.pack decoded data
        self.shift = shift  # Offset of the bit mask within the struct.pack decoded data
        self.map = map  # Mapping table of field values to enumeration values
        self.xlate = xlate  # Tuple of translator functions (decode, encode) for complex fields
        
    
    # Get the decoded representation of the attribute
    def get(self, sensor):
        # Decode the bytes the contain the field
        d = struct.unpack(self.format, sensor.getDataField(self.page, self.offset, self.size))[0]
        # Shift the resulting integer to align the field at the least significant bit
        if self.shift != 0: d >>= self.shift
        # Apply the bit mask to limit the field's width
        if self.mask != 0: d &= self.mask
        # Map to enumeration values (if present)
        if self.map is not None: d = self.map[d]
        # Call decoding translator function (if present)
        if self.xlate is not None: d = self.xlate[0](d)
        # Return the result
        return d


    # Set the attribute's value to the passed data (which needs to be encoded first)
    def set(self, sensor, data):
        # Call encoding translator function (if present)
        if self.xlate is not None: data = self.xlate[1](data)
        # If there is an enumeration value map, find the matching element and store the key.
        if self.map is not None:
            data = str(data)
            for k, v in self.map.items():
                if str(v) == data:
                    data = k
                    break
            else: raise Exception("Could not find %s in attribute value map." % data)
        # Otherwise just parse the data as an integer
        else: data = int(data)
        # Decode the old contents of the bytes that contain the field
        d = struct.unpack(self.format, sensor.getDataField(self.page, self.offset, self.size))[0]
        # Determine the bit mask to overwrite (if mask is set to 0, overwrite everything)
        m = self.mask if self.mask != 0 else -1
        # Zero the bits storing this attribute
        d &= ~(m << self.shift)
        # Insert the attribute data at the correct position
        d |= (data & m) << self.shift
        # Encode the new byte contents and write them to the configuration page
        sensor.setDataField(self.page, self.offset, struct.pack(self.format, d))

        

# Base class representing a sensor
class Sensor(object):

    # Base class constructor: Populate attributes and other basic information
    def __init__(self, device, id):
        # Generic attributes, shared by every sensor type
        self.attrs = {
            "vendor": Attribute(0, 0, "<I", "%08X"),
            "product": Attribute(0, 4, "<I", "%08X"),
            "serial": Attribute(0, 8, "<I", "%08X"),
            "formatVendor": Attribute(0, 12, "<I", "%08X"),
            "formatType": Attribute(0, 16, "<H", "%04X"),
            "formatVersion": Attribute(0, 18, "B", "%02X"),
            "recordSize": Attribute(0, 19, "B", "%d bits"),
            "scheduleOffset": Attribute(0, 20, "<I", "%d µs"),
            "scheduleInterval": Attribute(0, 24, "<I", "%d µs"),
        }
        # Sensor node that this sensor is attached to
        self.device = device
        # Sensor ID within the sensor node
        self.id = id
        # A textual representation of the sensor type (should be overridden by subclasses)
        self.name = "UNKNOWN(%08X:%08X)" % (self.getAttr("vendor"), self.getAttr("product"))
    
    
    # Fetch the contents of a configuration page of this sensor from the node driver's cache
    def getDataPage(self, page):
        return self.device.sensorDataCache[self.id][page]
    
    
    # Flush the node driver's cache of a configuration page of this sensor
    def updateDataPage(self, page):
        result, data = self.device.cmd(0x0104, (self.id << 2) | page)
        if result != 0: raise Exception("Device returned status code %02X" % result)
        self.device.sensorDataCache[self.id][page] = data
        self.device.sensorDataDirty[self.id][page] = False
        return data
    

    # Write the passed data to a configuration page of this sensor and send it to the node
    def writeDataPage(self, page, data):
        result, data = self.device.cmd(0x0105, (self.id << 2) | page, data)
        if result != 0: raise Exception("Device returned status code %02X" % result)
        self.device.sensorDataCache[self.id][page] = data
        self.device.sensorDataDirty[self.id][page] = False
        return data

        
    # Send all dirty configuration pages of this sensor to the node
    def commitDataPages(self):
        for page in range(4):
            if self.device.sensorDataDirty[self.id][page]:
                self.writeDataPage(page, self.device.sensorDataCache[self.id][page])

          
    # Get a byte range from a configuration page of this sensor
    def getDataField(self, page, offset, size):
        return self.device.sensorDataCache[self.id][page][offset : offset + size]

          
    # Overwrite a byte range of a configuration page of this sensor
    def setDataField(self, page, offset, data):
        # Ignore the request if the data matches the old contents
        if data == self.getDataField(page, offset, len(data)): return
        # Splice the new data into the page contents
        pagedata = self.device.sensorDataCache[self.id][page]
        pagedata = pagedata[:offset] + data + pagedata[offset + len(data):]
        # Write the new page contents into the cache and mark the page as dirty
        self.device.sensorDataCache[self.id][page] = pagedata
        self.device.sensorDataDirty[self.id][page] = True

        
    # Get the decoded representation of an attribute of this sensor
    def getAttr(self, attr):
        attr = self.attrs[attr]
        return attr.get(self)

        
    # Set an attribute of this sensor to the passed value
    def setAttr(self, attr, data):
        attr = self.attrs[attr]
        return attr.set(self, data)

        

# Base class for a sensor data decoder
class Decoder(object):

    # Base class constructor: Populate generic class members
    def __init__(self, device, sensor):
        self.device = device  # Sensor node that the sensor is attached to
        self.sensor = sensor  # Sensor driver
        self.offset = 0  # The sensor's measurement schedule offset
        self.interval = 0  # The sensor's measurement schedule interval
        self.recordBytes = 0  # The number of bytes per data point of this sensor
        self.printAs = "UNKNOWN(NO_DECODER)"  # String formatting pattern how to print measurements
        self.component = ()  # Tuple of symbols of the components of each data point
        self.unit = ()  # Tuple of units of the components of each data point
    
    
    # Apply sensor configuration to the decoder
    def update(self):
        # Common to all sensors are the schedule attributes and the data point size
        self.offset = self.sensor.getAttr("scheduleOffset")
        self.interval = self.sensor.getAttr("scheduleInterval")
        self.recordBytes = self.sensor.getAttr("recordSize") // 8
        
        
    # Decode a captured data point, passed as a binary string
    def decode(self, sample):
        # If there is no sensor driver that overrides this, we don't know how
        # to decode this sensor's measurements, so just ignore and skip them.
        return ()
