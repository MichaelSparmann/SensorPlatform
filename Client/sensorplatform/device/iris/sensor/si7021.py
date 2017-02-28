# Si7020/7021 hygrometer/thermometer sensor driver
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



# Si7020/7021 sensor driver
class HumiditySensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Relative Humidity (Si7021/Si7020)"
        self.attrs["resolution"] = Attribute(2, 0, "B", mask=3, shift=5, map={0:"h12t14", 1:"h8t12", 2:"h10t13", 3:"h11t11"})
        self.attrs["heaterOn"] = Attribute(2, 0, "B", mask=1, shift=4)
        self.attrs["heaterCurrent"] = Attribute(2, 0, "B", mask=15, shift=0)
        # Which channels of the sensor shall be sampled
        self.attrs["enableTemperature"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableHumidity"] = Attribute(2, 27, "B", mask=1, shift=0)

        

# Si7020/7021 measurement decoder
class HumidityDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "H: %7.3 %RH, Thyg: %7.3 °C"
        self.component = ("H", "Thyg")
        self.unit = ("%RH", "°C")
        # Which channels were actually sampled
        self.enableTemp = False
        self.enableHum = False
        
    def update(self):
        Decoder.update(self)
        self.enableTemp = self.sensor.getAttr("enableTemperature")
        self.enableHum = self.sensor.getAttr("enableHumidity")

    def decode(self, sample):
        # See datasheet for details
        if self.enableHum:
            hum = struct.unpack(">H", sample[:2])[0] * 125. / 65536. - 6.
            sample = sample[2:]
        else: hum = float("NaN")
        if self.enableTemp: temp = struct.unpack(">H", sample)[0] * 175.72 / 65535. - 46.85
        else: temp = float("NaN")
        return (hum, temp)
