# Virtual timing sensor driver
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


from sensorplatform.device.iris.sensor.base import Attribute, Sensor, Decoder
import struct



# Virtual timing sensor driver
class TimingSensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Timing (virtual)"
        # Which channels of the sensor shall be sampled
        self.attrs["enableMasterTime"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableLocalTime"] = Attribute(2, 27, "B", mask=1, shift=0)

        

# Timing measurement decoder
class TimingDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # The next 3 fields are dynamically populated in update()
        self.printAs = ""
        self.component = ()
        self.unit = ()
        # Bit mask of actually sampled channels
        self.enableBits = 0
        # Field names for each bit (LSB first)
        self.fieldNames = ("LocalTime", "MasterTime")
        
    def update(self):
        Decoder.update(self)
        self.enableBits = struct.unpack("B", self.sensor.getDataField(2, 27, 1))[0]
        self.printAs = ""
        self.component = []
        self.unit = []
        for i in range(2):
            if (self.enableBits >> i) & 1:
                self.printAs += ", %s: %%d" % self.fieldNames[i]
                self.component.append(self.fieldNames[i])
                self.unit.append("µs")

    def decode(self, sample):
        return struct.unpack("<%dH" % (len(sample) / 2), sample)
