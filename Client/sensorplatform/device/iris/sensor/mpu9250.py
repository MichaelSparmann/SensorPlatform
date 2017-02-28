# MPU-9250/6050 inertial measurement unit sensor driver
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



# MPU-9250/6050 accelerometer sensor driver
class AccelSensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Force 3D Vector (MPU9250/MPU6250)"
        # See datasheet for the meaning of these attributes:
        # Calibration values
        self.attrs["stDataX"] = Attribute(1, 0, "b")
        self.attrs["stDataY"] = Attribute(1, 1, "b")
        self.attrs["stDataZ"] = Attribute(1, 2, "b")
        # Configuration
        self.attrs["selfTestX"] = Attribute(2, 0, "B", mask=1, shift=7)
        self.attrs["selfTestY"] = Attribute(2, 0, "B", mask=1, shift=6)
        self.attrs["selfTestZ"] = Attribute(2, 0, "B", mask=1, shift=5)
        self.attrs["fullScale"] = Attribute(2, 0, "B", mask=3, shift=3, map={0:2, 1:4, 2:8, 3:16})
        self.attrs["fchoiceB"] = Attribute(2, 1, "B", mask=1, shift=3)
        self.attrs["dlpfCfg"] = Attribute(2, 1, "B", mask=7, shift=0)
        # Which channels of the sensor shall be sampled
        self.attrs["enableX"] = Attribute(2, 27, "B", mask=1, shift=2)
        self.attrs["enableY"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableZ"] = Attribute(2, 27, "B", mask=1, shift=0)

        

# MPU-9250/6050 gyroscope sensor driver
class GyroSensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Angular Velocity 3D Vector (MPU9250/MPU6250)"
        # See datasheet for the meaning of these attributes:
        # Calibration values
        self.attrs["stDataX"] = Attribute(1, 0, "b")
        self.attrs["stDataY"] = Attribute(1, 1, "b")
        self.attrs["stDataZ"] = Attribute(1, 2, "b")
        # Configuration
        self.attrs["offsetX"] = Attribute(2, 0, ">h")
        self.attrs["offsetY"] = Attribute(2, 2, ">h")
        self.attrs["offsetZ"] = Attribute(2, 4, ">h")
        self.attrs["sampleRateDiv"] = Attribute(2, 6, "B")
        self.attrs["dlpfCfg"] = Attribute(2, 7, "B", mask=7, shift=0)
        self.attrs["selfTestX"] = Attribute(2, 8, "B", mask=1, shift=7)
        self.attrs["selfTestY"] = Attribute(2, 8, "B", mask=1, shift=6)
        self.attrs["selfTestZ"] = Attribute(2, 8, "B", mask=1, shift=5)
        self.attrs["fullScale"] = Attribute(2, 8, "B", mask=3, shift=3, map={0:250, 1:500, 2:1000, 3:2000})
        self.attrs["fchoiceB"] = Attribute(2, 8, "B", mask=3, shift=0)
        # Which channels of the sensor shall be sampled
        self.attrs["enableX"] = Attribute(2, 27, "B", mask=1, shift=2)
        self.attrs["enableY"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableZ"] = Attribute(2, 27, "B", mask=1, shift=0)

        

# MPU-9250 magnetometer sensor driver
class MagSensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Magnetic Field 3D Vector (MPU9250/MPU6250)"
        # See datasheet for the meaning of these attributes:
        # Calibration values
        self.attrs["stDataX"] = Attribute(1, 0, "<h")
        self.attrs["stDataY"] = Attribute(1, 2, "<h")
        self.attrs["stDataZ"] = Attribute(1, 4, "<h")
        self.attrs["stOverflow"] = Attribute(1, 6, "B", mask=1, shift=3)
        self.attrs["calScaleX"] = Attribute(1, 7, "B")
        self.attrs["calScaleY"] = Attribute(1, 8, "B")
        self.attrs["calScaleZ"] = Attribute(1, 9, "B")
        # Which channels of the sensor shall be sampled
        self.attrs["enableX"] = Attribute(2, 27, "B", mask=1, shift=2)
        self.attrs["enableY"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableZ"] = Attribute(2, 27, "B", mask=1, shift=0)

        

# MPU-9250/6050 temperature sensor driver
class TempSensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Temperature (MPU9250/MPU6250)"

        
        
# MPU-9250/6050 accelerometer measurement decoder
class AccelDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "X: %6.3f g, Y: %6.3f g, Z: %6.3f g"
        self.component = ("X", "Y", "Z")
        self.unit = ("g", "g", "g")
        # Which channels were actually sampled
        self.enable = (False, False, False)
        self.factor = 0
        
    def update(self):
        Decoder.update(self)
        self.enable = (self.sensor.getAttr("enableX"),
                       self.sensor.getAttr("enableY"),
                       self.sensor.getAttr("enableZ"))
        self.factor = self.sensor.getAttr("fullScale") / 32767.
        
    def decode(self, sample):
        result = []
        for on in self.enable:
            if on:
                result.append(struct.unpack(">h", sample[:2])[0] * self.factor)
                sample = sample[2:]
            else: result.append(float("NaN"))
        return tuple(result)

        
        
# MPU-9250/6050 gyroscope measurement decoder
class GyroDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "X: %8.2f °/s, Y: %8.2f °/s, Z: %8.2f °/s"
        self.component = ("X", "Y", "Z")
        self.unit = ("°/s", "°/s", "°/s")
        # Which channels were actually sampled
        self.enable = (False, False, False)
        self.factor = 0
        
    def update(self):
        Decoder.update(self)
        self.enable = (self.sensor.getAttr("enableX"),
                       self.sensor.getAttr("enableY"),
                       self.sensor.getAttr("enableZ"))
        self.factor = self.sensor.getAttr("fullScale") / 32767.
        
    def decode(self, sample):
        result = []
        for on in self.enable:
            if on:
                result.append(struct.unpack(">h", sample[:2])[0] * self.factor)
                sample = sample[2:]
            else: result.append(float("NaN"))
        return tuple(result)

        
        
# MPU-9250 magnetometer measurement decoder
class MagDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "X: %8.2f µT, Y: %8.2f µT, Z: %8.2f µT"
        self.component = ("X", "Y", "Z")
        self.unit = ("µT", "µT", "µT")
        # Which channels were actually sampled
        self.enable = (False, False, False)
        self.factor = (0, 0, 0)
        
    def update(self):
        Decoder.update(self)
        self.enable = (self.sensor.getAttr("enableX"),
                       self.sensor.getAttr("enableY"),
                       self.sensor.getAttr("enableZ"))
        self.factor = (0.15 * (0.5 + self.sensor.getAttr("calScaleX") / 256.),
                       0.15 * (0.5 + self.sensor.getAttr("calScaleY") / 256.),
                       0.15 * (0.5 + self.sensor.getAttr("calScaleZ") / 256.))
        
    def decode(self, sample):
        result = []
        for i in range(3):
            if self.enable[i]:
                result.append(struct.unpack(">h", sample[:2])[0] * self.factor[i])
                sample = sample[2:]
            else: result.append(float("NaN"))
        return tuple(result)


        
# MPU-9250/6050 temperature measurement decoder
class TempDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "Timu: %6.2f °C"
        self.component = ("Timu",)
        self.unit = ("°C",)
        
    def decode(self, sample):
        return (struct.unpack(">h", sample)[0] / 333.87 + 21,)

