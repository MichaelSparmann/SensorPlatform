// Ambient light and proximity sensor (APDS-9901) driver
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
#include "light.h"
#include "sys/util.h"
#include "../common.h"
#include "../i2c.h"


namespace Light
{
    const SensorType intensitySensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b1111110000000000000000000001,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x494c0199,
        IntensitySensor::init, IntensitySensor::verify, IntensitySensor::start, IntensitySensor::stop,
    };

    IntensitySensor intensitySensor(SensorId_LightIntensity);
    bool enableVisible;  // Whether or not to sample visible light intensity
    bool enableInfrared;  // Whether or not to sample infrared light intensity
    bool enableReflected;  // Whether or not to sample reflected light intensity


    void IntensitySensor::init(Sensor* sensor, bool first)
    {
        uint8_t reg;
        // Read ID register and compare it against the expected value (0x20) to check presence
        I2C::Result result = LIGHT_I2C_BUS.readRegs(0x39, 0xb2, &reg, 1);
        sensor->present = result == I2C::RESULT_OK && reg == 0x20;
        if (!sensor->present) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x0199;
        info->info.formatVersion = 0;
    }


    void IntensitySensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Cache enabled channels (before sensor configuration is flushed from data buffer)
        enableVisible = info->data[1].u8[27] & 1;
        enableInfrared = (info->data[1].u8[27] >> 1) & 1;
        enableReflected = (info->data[1].u8[27] >> 2) & 1;
        // Calculate total sample size
        info->info.recordSize = (enableVisible + enableInfrared + enableReflected) * 16;
    }


    void IntensitySensor::start(Sensor* sensor, int time)
    {
        if (!sensor->interval || (!enableVisible && !enableInfrared && !enableReflected)) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        info->data[1].u8[2] = (info->data[1].u8[2] & 0xc3) | 0x20;  // Proximity channel 1, 1x gain
        LIGHT_I2C_BUS.writeRegs(0x39, 0xad, info->data[1].u8, 3);  // Write CONFIG, PPCOUNT, CONTROL
        LIGHT_I2C_BUS.writeRegs(0x39, 0xa1, info->data[1].u8 + 3, 3);  // Write ATIME, PTIME, WTIME
        uint8_t reg = (info->data[1].u8[27] & 0xe) | ((info->data[1].u8[27] & 1) << 1) | 1;
        LIGHT_I2C_BUS.writeRegs(0x39, 0xa0, &reg, sizeof(reg));  // Write ENABLE
    }


    void IntensitySensor::stop(Sensor* sensor)
    {
        uint8_t reg = 0;
        LIGHT_I2C_BUS.writeRegs(0x39, 0xa0, &reg, sizeof(reg));  // Clear ENABLE
    }


    void IntensitySensor::capture(Sensor* sensor)
    {
        struct __attribute__((packed,aligned(2)))
        {
            uint16_t visible;
            uint16_t infrared;
            uint16_t reflected;
        } buf;
        // Read measurement value from sensor
        LIGHT_I2C_BUS.readRegs(0x39, 0xb4, &buf, sizeof(buf));
        // Record the requested channels
        if (enableVisible) SensorTask::writeMeasurement(buf.visible);
        if (enableInfrared) SensorTask::writeMeasurement(buf.infrared);
        if (enableReflected) SensorTask::writeMeasurement(buf.reflected);
        // Schedule capturing of next sample
        sensor->reschedule(&sensor->captureTask);
    }
}
