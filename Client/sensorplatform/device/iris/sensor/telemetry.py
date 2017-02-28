# Virtual telemetry sensor driver
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



# Virtual telemnetry sensor driver
class TelemetrySensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Telemetry (virtual)"
        # Which channels of the sensor shall be sampled
        self.attrs["enableReserved1"] = Attribute(2, 27, "B", mask=1, shift=7)
        self.attrs["enableReserved0"] = Attribute(2, 27, "B", mask=1, shift=6)
        self.attrs["enableRXCMDCount"] = Attribute(2, 27, "B", mask=1, shift=5)
        self.attrs["enableTXACKCount"] = Attribute(2, 27, "B", mask=1, shift=4)
        self.attrs["enableTXAttemptCount"] = Attribute(2, 27, "B", mask=1, shift=3)
        self.attrs["enableSOFDiscontinuity"] = Attribute(2, 27, "B", mask=1, shift=2)
        self.attrs["enableSOFTimingFailed"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableSOFReceived"] = Attribute(2, 27, "B", mask=1, shift=0)

        

# Telemetry measurement decoder
class TelemetryDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # The next 3 fields are dynamically populated in update()
        self.printAs = ""
        self.component = ()
        self.unit = ()
        # Bit mask of actually sampled channels
        self.enableBits = 0
        # Field names for each bit (LSB first)
        self.fieldNames = ("SOFReceived", "SOFTimingFailed", "SOFDiscontinuity", "TXAttemptCount",
                           "TXACKCount", "RXCMDCount", "Reserved0", "Reserved1")
        
    def update(self):
        Decoder.update(self)
        self.enableBits = struct.unpack("B", self.sensor.getDataField(2, 27, 1))[0]
        self.printAs = ""
        self.component = []
        self.unit = []
        for i in range(8):
            if (self.enableBits >> i) & 1:
                self.printAs += ", %s: %%d" % self.fieldNames[i]
                self.component.append(self.fieldNames[i])
                self.unit.append("")

    def decode(self, sample):
        return struct.unpack("<%dH" % (len(sample) / 2), sample)
