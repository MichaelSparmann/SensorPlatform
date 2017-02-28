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
#include "baro.h"
#include "sys/util.h"
#include "../common.h"
#include "../i2c.h"


namespace Baro
{
    const SensorType pressureSensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b1110000000000000000000000000,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x525080b2,
        PressureSensor::init, PressureSensor::verify, PressureSensor::start, PressureSensor::stop,
    };

    PressureSensor pressureSensor(SensorId_BaroPressure);
    bool highres;  // Whether or not to record 24-bit samples (instead of 16-bit)


    void PressureSensor::init(Sensor* sensor, bool first)
    {
        uint8_t reg;
        // Check presence (ID register should read 0x58)
        I2C::Result result = BARO_I2C_BUS.readRegs(0x76, 0xd0, &reg, 1);
        sensor->present = result == I2C::RESULT_OK && reg == 0x58;
        if (!sensor->present) return;
        // Read calibration data from the sensor
        BARO_I2C_BUS.readRegs(0x76, 0x88, sensor->getInfoPtr()->data, 24);
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x80b2;
        info->info.formatVersion = 0;
    }


    void PressureSensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Calculate sample size (16- or 24-bit sampling) for temperature and pressure each
        info->info.recordSize = 32 + (info->data[1].u8[2] & 1) * 16;
    }


    void PressureSensor::start(Sensor* sensor, int time)
    {
        if (!sensor->interval) return;
        SeriesHeader::SensorInfo* info = pressureSensor.getInfoPtr();
        info->data[1].u8[0] |= 3;  // Mode: Normal
        BARO_I2C_BUS.writeRegs(0x76, 0xf4, info->data[1].u8, 2);  // Write CTRL_MEAS register
        // Cache sample recording mode (before sensor configuration is flushed from data buffer)
        highres = info->data[1].u8[2] & 1;
    }


    void PressureSensor::stop(Sensor* sensor)
    {
        uint8_t zero = 0;
        BARO_I2C_BUS.writeRegs(0x76, 0xf4, &zero, sizeof(zero));  // Clear CTRL_MEAS register
    }


    void PressureSensor::capture(Sensor* sensor)
    {
        struct __attribute__((packed,aligned(2)))
        {
            uint16_t pressHigh;
            uint8_t pressLow;
            uint16_t tempHigh;
            uint8_t tempLow;
        } buf;
        // Read measurement value from sensor
        BARO_I2C_BUS.readRegs(0x76, 0xf7, &buf, sizeof(buf));
        // Record most significant 16 bits of pressure and temperature
        SensorTask::writeMeasurement(buf.pressHigh);
        SensorTask::writeMeasurement(buf.tempHigh);
        // Record least significant 8 bits of pressure and temperature if enabled
        if (highres) SensorTask::writeMeasurement((buf.tempLow << 8) | buf.pressLow);
        // Schedule capturing of next sample
        sensor->reschedule(&sensor->captureTask);
    }
}
