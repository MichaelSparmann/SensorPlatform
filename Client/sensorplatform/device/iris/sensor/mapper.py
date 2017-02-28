# Sensor type to driver mapper
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
import sensorplatform.device.iris.sensor.timing
import sensorplatform.device.iris.sensor.telemetry
import sensorplatform.device.iris.sensor.mpu9250
import sensorplatform.device.iris.sensor.bmp280
import sensorplatform.device.iris.sensor.si7021
import sensorplatform.device.iris.sensor.apds9901


# Mapping table: (SensorVendor, SensorProduct) => Sensor subclass
sensorTypeMap = {
    0x53414149: {
        0x49544956: sensorplatform.device.iris.sensor.timing.TimingSensor,
        0x45544956: sensorplatform.device.iris.sensor.telemetry.TelemetrySensor,
        0x43415092: sensorplatform.device.iris.sensor.mpu9250.AccelSensor,
        0x59475092: sensorplatform.device.iris.sensor.mpu9250.GyroSensor,
        0x474d5092: sensorplatform.device.iris.sensor.mpu9250.MagSensor,
        0x4d545092: sensorplatform.device.iris.sensor.mpu9250.TempSensor,
        0x525080b2: sensorplatform.device.iris.sensor.bmp280.PressureSensor,
        0x4d482170: sensorplatform.device.iris.sensor.si7021.HumiditySensor,
        0x494c0199: sensorplatform.device.iris.sensor.apds9901.IntensitySensor,
    }
}


# Mapping table: (DataFormatVendor, DataFormatType) => Decoder subclass
dataFormatMap = {
    0x53414149: {
        0x4954: sensorplatform.device.iris.sensor.timing.TimingDecoder,
        0x4554: sensorplatform.device.iris.sensor.telemetry.TelemetryDecoder,
        0x5092: sensorplatform.device.iris.sensor.mpu9250.AccelDecoder,
        0x5192: sensorplatform.device.iris.sensor.mpu9250.GyroDecoder,
        0x5292: sensorplatform.device.iris.sensor.mpu9250.MagDecoder,
        0x5392: sensorplatform.device.iris.sensor.mpu9250.TempDecoder,
        0x80b2: sensorplatform.device.iris.sensor.bmp280.PressureDecoder,
        0x2170: sensorplatform.device.iris.sensor.si7021.HumidityDecoder,
        0x0199: sensorplatform.device.iris.sensor.apds9901.IntensityDecoder,
    }
}


# Instantiate the correct driver and decoder object for a given sensor information page
def instantiate(device, id):
    # Instantiate the driver (handles configuration etc.)
    vendor, product = struct.unpack("<II", device.sensorDataCache[id][0][:8])
    c = sensorplatform.device.iris.sensor.base.Sensor
    if vendor in sensorTypeMap and product in sensorTypeMap[vendor]:
        c = sensorTypeMap[vendor][product]
    s = c(device, id)
    # Instantiate the decoder (handles measurement data decoding)
    vendor, type = struct.unpack("<IH", device.sensorDataCache[id][0][12:18])
    c = sensorplatform.device.iris.sensor.base.Decoder
    if vendor in dataFormatMap and type in dataFormatMap[vendor]:
        c = dataFormatMap[vendor][type]
    s.decoder = c(device, s)
    return s
