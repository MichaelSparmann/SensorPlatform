# APDS-9901 ambient light and proximity sensor driver
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



# APDS-9901 sensor driver
class IntensitySensor(Sensor):
    def __init__(self, device, id):
        Sensor.__init__(self, device, id)
        self.name = "Light Intensity (APDS-9901)"
        # See datasheet for the meaning of these attributes
        self.attrs["waitTimeFactor"] = Attribute(2, 0, "B", mask=1, shift=1, map={0:1, 1:12})
        self.attrs["reflectedPulseCount"] = Attribute(2, 1, "B")
        self.attrs["reflectedPulseCurrent"] = Attribute(2, 2, "B", mask=3, shift=6, map={0:100, 1:50, 2:25, 3:12.5})
        self.attrs["ambientSensorGain"] = Attribute(2, 2, "B", mask=3, shift=0, map={0:1, 1:8, 2:16, 3:120})
        self.attrs["ambientIntegrationTime"] = Attribute(2, 3, "B", xlate=(lambda x: (256 - x) * 2720, lambda x: int(max(0, min(255, 256 - int(x) / 2720)))))
        self.attrs["reflectedIntegrationTime"] = Attribute(2, 4, "B", xlate=(lambda x: (256 - x) * 2720, lambda x: int(max(0, min(255, 256 - int(x) / 2720)))))
        self.attrs["waitTime"] = Attribute(2, 5, "B", xlate=(lambda x: (256 - x) * 2720, lambda x: int(max(0, min(255, 256 - int(x) / 2720)))))
        self.attrs["enableWait"] = Attribute(2, 27, "B", mask=1, shift=3)
        # Which channels of the sensor shall be sampled
        self.attrs["enableReflected"] = Attribute(2, 27, "B", mask=1, shift=2)
        self.attrs["enableInfrared"] = Attribute(2, 27, "B", mask=1, shift=1)
        self.attrs["enableFullSpectrum"] = Attribute(2, 27, "B", mask=1, shift=0)

        
        
# APDS-9901 measurement decoder
class IntensityDecoder(Decoder):
    def __init__(self, device, sensor):
        Decoder.__init__(self, device, sensor)
        # Measurement channels
        self.printAs = "ADfs: %5d, ADir: %5d, ADrf: %5d, Ev: %6dlx"
        self.component = ("ADfs", "ADir", "ADrf", "Ev")
        self.unit = ("counts", "counts", "counts", "lx")
        # Which channels were actually sampled
        self.enableFullSpectrum = False
        self.enableInfrared = False
        self.enableReflected = False
        self.factor = 0
        
    def update(self):
        Decoder.update(self)
        self.enableFullSpectrum = self.sensor.getAttr("enableFullSpectrum")
        self.enableInfrared = self.sensor.getAttr("enableInfrared")
        self.enableReflected = self.sensor.getAttr("enableReflected")
        # See datasheet for details
        self.factor = 24960. / self.sensor.getAttr("ambientIntegrationTime") / self.sensor.getAttr("ambientSensorGain")

    def decode(self, sample):
        if self.enableFullSpectrum:
            adfs = struct.unpack("<H", sample[:2])[0]
            sample = sample[2:]
        else: adfs = float("NaN")
        if self.enableInfrared:
            adir = struct.unpack("<H", sample[:2])[0]
            sample = sample[2:]
        else: adir = float("NaN")
        if self.enableReflected: adrf = struct.unpack("<H", sample)[0]
        else: adrf = float("NaN")
        # See datasheet for details
        lux = max(adfs - 2.23 * adir, 0.7 * adfs - 1.42 * adir) * self.factor
        return (adfs, adir, adrf, lux)
