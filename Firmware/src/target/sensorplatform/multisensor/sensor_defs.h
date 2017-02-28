// Sensor Node Sensor List and ID Mapping
// Copyright (C) 2016-2017 Michael Sparmann
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


/* 00 */ DEFINE_SENSOR(Timing, Timing::timingSensor)
/* 01 */ DEFINE_SENSOR(Telemetry, Telemetry::telemetrySensor)
/* 02 */ DEFINE_SENSOR(reserved02, *((Sensor*)NULL))
/* 03 */ DEFINE_SENSOR(reserved03, *((Sensor*)NULL))
/* 04 */ DEFINE_SENSOR(reserved04, *((Sensor*)NULL))
/* 05 */ DEFINE_SENSOR(reserved05, *((Sensor*)NULL))
/* 06 */ DEFINE_SENSOR(reserved06, *((Sensor*)NULL))
/* 07 */ DEFINE_SENSOR(reserved07, *((Sensor*)NULL))
/* 08 */ DEFINE_SENSOR(reserved08, *((Sensor*)NULL))
/* 09 */ DEFINE_SENSOR(reserved09, *((Sensor*)NULL))
/* 10 */ DEFINE_SENSOR(reserved10, *((Sensor*)NULL))
/* 11 */ DEFINE_SENSOR(reserved11, *((Sensor*)NULL))
/* 12 */ DEFINE_SENSOR(reserved12, *((Sensor*)NULL))
/* 13 */ DEFINE_SENSOR(reserved13, *((Sensor*)NULL))
/* 14 */ DEFINE_SENSOR(reserved14, *((Sensor*)NULL))
/* 15 */ DEFINE_SENSOR(reserved15, *((Sensor*)NULL))
/* 16 */ DEFINE_SENSOR(IMUAccel, IMU::accelSensor)
/* 17 */ DEFINE_SENSOR(IMUGyro, IMU::gyroSensor)
/* 18 */ DEFINE_SENSOR(IMUMag, IMU::magSensor)
/* 19 */ DEFINE_SENSOR(IMUTemp, IMU::tempSensor)
/* 20 */ DEFINE_SENSOR(BaroPressure, Baro::pressureSensor)
/* 21 */ DEFINE_SENSOR(HygroHumidity, Hygro::humiditySensor)
/* 22 */ DEFINE_SENSOR(LightIntensity, Light::intensitySensor)
