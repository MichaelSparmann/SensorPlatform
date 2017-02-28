#pragma once

// Inertial measurement unit (MPU6050/MPU9250) driver
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


#include "global.h"
#include "sensor.h"


namespace IMU
{
    extern const SensorType accelSensorType;
    extern const SensorType gyroSensorType;
    extern const SensorType magSensorType;
    extern const SensorType tempSensorType;

    class AccelSensor : public Sensor
    {
    public:
        bool enableX;
        bool enableY;
        bool enableZ;
        static void init(Sensor* sensor, bool first);
        static void verify(Sensor* sensor, SeriesHeader::SensorInfo* info);
        static void start(Sensor* sensor, int time);
        static void stop(Sensor* sensor);
        static void capture(Sensor* sensor);
        constexpr AccelSensor(uint8_t id) : Sensor(&accelSensorType, id, capture), enableX(0), enableY(0), enableZ(0) {}
    };

    class GyroSensor : public Sensor
    {
    public:
        bool enableX;
        bool enableY;
        bool enableZ;
        static void init(Sensor* sensor, bool first);
        static void verify(Sensor* sensor, SeriesHeader::SensorInfo* info);
        static void start(Sensor* sensor, int time);
        static void stop(Sensor* sensor);
        static void capture(Sensor* sensor);
        constexpr GyroSensor(uint8_t id) : Sensor(&gyroSensorType, id, capture), enableX(0), enableY(0), enableZ(0) {}
    };

    class MagSensor : public Sensor
    {
    public:
        bool enableX;
        bool enableY;
        bool enableZ;
        static void init(Sensor* sensor, bool first);
        static void verify(Sensor* sensor, SeriesHeader::SensorInfo* info);
        static void start(Sensor* sensor, int time);
        static void stop(Sensor* sensor);
        static void capture(Sensor* sensor);
        constexpr MagSensor(uint8_t id) : Sensor(&magSensorType, id, capture), enableX(0), enableY(0), enableZ(0) {}
    };

    class TempSensor : public Sensor
    {
    public:
        static void init(Sensor* sensor, bool first);
        static void verify(Sensor* sensor, SeriesHeader::SensorInfo* info);
        static void start(Sensor* sensor, int time);
        static void stop(Sensor* sensor);
        static void capture(Sensor* sensor);
        constexpr TempSensor(uint8_t id) : Sensor(&tempSensorType, id, capture) {}
    };

    extern int bootedAt;
    extern AccelSensor accelSensor;
    extern GyroSensor gyroSensor;
    extern MagSensor magSensor;
    extern TempSensor tempSensor;

    extern void powerDown();
}
