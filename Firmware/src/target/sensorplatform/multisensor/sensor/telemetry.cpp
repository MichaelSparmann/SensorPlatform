// Virtual telemetry sensor driver
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
#include "telemetry.h"
#include "sys/util.h"
#include "../common.h"
#include "../i2c.h"
#include "../radio.h"


namespace Telemetry
{
    const SensorType telemetrySensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b0000000000000000000000000001,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x45544956,
        TelemetrySensor::init, TelemetrySensor::verify, TelemetrySensor::start, TelemetrySensor::stop,
    };

    TelemetrySensor telemetrySensor(SensorId_Telemetry);
    uint8_t enableMask;  // Bit mask of enabled channels


    void TelemetrySensor::init(Sensor* sensor, bool first)
    {
        sensor->present = true;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x4554;
        info->info.formatVersion = 0;
    }


    void TelemetrySensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Count enabled channels and determine total sample size
        enableMask = info->data[1].u8[27];
        int enabled = 0;
        for (uint32_t bits = enableMask; bits; bits >>= 1)
            if (bits & 1)
                enabled++;
        info->info.recordSize = enabled * 16;
    }


    void TelemetrySensor::start(Sensor* sensor, int time)
    {
    }


    void TelemetrySensor::stop(Sensor* sensor)
    {
    }


    void TelemetrySensor::capture(Sensor* sensor)
    {
        // Record the requested channels
        uint16_t* data = (uint16_t*)&Radio::noDataResponse.telemetry;
        for (uint32_t bits = enableMask, i = 0; bits; bits >>= 1, i++)
            if (bits & 1)
                SensorTask::writeMeasurement(data[i]);
        // Schedule capturing of next sample
        sensor->reschedule(&sensor->captureTask);
    }
}
