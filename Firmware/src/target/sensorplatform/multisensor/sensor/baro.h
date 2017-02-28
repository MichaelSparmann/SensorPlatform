#pragma once

// Barometer (BMP280) driver
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


namespace Baro
{
    extern const SensorType pressureSensorType;

    class PressureSensor : public Sensor
    {
    public:
        static void init(Sensor* sensor, bool first);
        static void verify(Sensor* sensor, SeriesHeader::SensorInfo* info);
        static void start(Sensor* sensor, int time);
        static void stop(Sensor* sensor);
        static void capture(Sensor* sensor);
        constexpr PressureSensor(uint8_t id) : Sensor(&pressureSensorType, id, capture) {}
    };

    extern PressureSensor pressureSensor;
}
