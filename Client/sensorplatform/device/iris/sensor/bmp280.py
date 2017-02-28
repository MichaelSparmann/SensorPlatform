# BMP-280 barometer sensor driver
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



# BMP-280 sensor driver
class PressureSensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Air Pressure (BMP280)"
        # See datasheet for the meaning of these attributes:
        # Calibration values
        self.attrs["calT1"] = Attribute(1, 0, "<H")
        self.attrs["calT2"] = Attribute(1, 2, "<h")
        self.attrs["calT3"] = Attribute(1, 4, "<h")
        self.attrs["calP1"] = Attribute(1, 6, "<H")
        self.attrs["calP2"] = Attribute(1, 8, "<h")
        self.attrs["calP3"] = Attribute(1, 10, "<h")
        self.attrs["calP4"] = Attribute(1, 12, "<h")
        self.attrs["calP5"] = Attribute(1, 14, "<h")
        self.attrs["calP6"] = Attribute(1, 16, "<h")
        self.attrs["calP7"] = Attribute(1, 18, "<h")
        self.attrs["calP8"] = Attribute(1, 20, "<h")
        self.attrs["calP9"] = Attribute(1, 22, "<h")
        # Configuration
        self.attrs["temperatureOversampling"] = Attribute(2, 0, "B", mask=7, shift=5, map={0:0, 1:1, 2:2, 3:4, 4:8, 5:16})
        self.attrs["pressureOversampling"] = Attribute(2, 0, "B", mask=7, shift=2, map={0:0, 1:1, 2:2, 3:4, 4:8, 5:16})
        self.attrs["standbyTime"] = Attribute(2, 1, "B", mask=7, shift=5, map={0:500, 1:62500, 2:125000, 3:250000, 4:500000, 5:1000000, 6:2000000, 7:4000000})
        self.attrs["filterWeight"] = Attribute(2, 1, "B", mask=7, shift=2, map={0:0, 1:1, 2:3, 3:7, 4:15})
        self.attrs["resolution"] = Attribute(2, 2, "B", mask=1, shift=0, map={0:16, 1:20})

        

# BMP-280 measurement decoder
class PressureDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "P: %8.1 Pa, Tbaro: %7.3 °C"
        self.component = ("P", "Tbaro")
        self.unit = ("Pa", "°C")
        # Calibration values
        self.T = ()
        self.P = ()
        
    def update(self):
        Decoder.update(self)
        self.T = (self.sensor.getAttr("calT1"), self.sensor.getAttr("calT2"), self.sensor.getAttr("calT3"))
        self.P = (self.sensor.getAttr("calP1"), self.sensor.getAttr("calP2"), self.sensor.getAttr("calP3"),
                  self.sensor.getAttr("calP4"), self.sensor.getAttr("calP5"), self.sensor.getAttr("calP6"),
                  self.sensor.getAttr("calP7"), self.sensor.getAttr("calP8"), self.sensor.getAttr("calP9"))

    def decode(self, sample):
        press, temp = struct.unpack(">HH", sample[:4])
        # In 20-bit resolution mode, the two LSBs for pressure and temperature follow
        if len(sample) == 6: pressLow, tempLow = struct.unpack("BB", sample[4:])
        else: pressLow, tempLow = 0, 0
        # Get raw 20-bit measurement values
        press, temp = (press << 4) | (pressLow >> 4), (temp << 4) | (tempLow >> 4)
        # Convert and temperature compensate as specified by the datasheet
        temp = (temp / 16. - self.T[0]) / 1024.
        temp = temp * self.T[1] + temp * temp / 64. * self.T[2]
        var1 = (temp / 2.) - 64000.
        var2 = var1 * self.P[4] + var1 * var1 / 65536. * self.P[5] + self.P[3] * 131072.
        var1 = (1. + (var1 * self.P[1] + var1 * var1 / 524288.0 * self.P[2]) / 524288.0 / 32768.0) * self.P[0]
        if var1 != 0.:
            press = ((1048576. - press) - (var2 / 8192.)) * 6250. / var1
            press += (press * self.P[7] / 32768. + press * press / 2147483648. * self.P[8] + self.P[6]) / 16.
        else: press = float("NaN")
        return (press, temp / 5120.)
