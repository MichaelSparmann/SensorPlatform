// Hygrometer/Thermometer (Si7020/Si7021) driver
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
#include "hygro.h"
#include "sys/util.h"
#include "../common.h"
#include "../i2c.h"


namespace Hygro
{
    const SensorType humiditySensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b1000000000000000000000000001,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x4d482170,
        HumiditySensor::init, HumiditySensor::verify, HumiditySensor::start, HumiditySensor::stop,
    };

    HumiditySensor humiditySensor(SensorId_HygroHumidity);
    bool enableHum;  // Whether or not to sample humidity values
    bool enableTemp;  // Whether or not to sample temperature values


    // Send a command byte to the chip
    static void sendCmd(uint8_t cmd)
    {
        struct __attribute__((packed,aligned(4)))
        {
            I2C::Transaction info;
            I2C::Transaction::Transfer transfers[1];
        } txn =
        {
            I2C::Transaction(0x40, ARRAYLEN(txn.transfers)),
            {
                I2C::Transaction::Transfer(I2C::Transaction::Transfer::TYPE_TX, sizeof(cmd), &cmd),
            }
        };
        HYGRO_I2C_BUS.txn(&txn.info);
    }

    // Collect a response word (16 bits) from the chip
    static uint16_t collectResult()
    {
        uint16_t result;
        struct __attribute__((packed,aligned(4)))
        {
            I2C::Transaction info;
            I2C::Transaction::Transfer transfers[1];
        } txn =
        {
            I2C::Transaction(0x40, ARRAYLEN(txn.transfers)),
            {
                I2C::Transaction::Transfer(I2C::Transaction::Transfer::TYPE_RX, sizeof(result), &result),
            }
        };
        HYGRO_I2C_BUS.txn(&txn.info);
        return result;
    }


    void HumiditySensor::init(Sensor* sensor, bool first)
    {
        // Send multi-byte "Electronic Serial Number" command and collect the
        // least significant 32 bits of the result, which identify the chip type.
        const uint16_t cmd = 0xc9fc;
        uint32_t type;
        struct __attribute__((packed,aligned(4)))
        {
            I2C::Transaction info;
            I2C::Transaction::Transfer transfers[2];
        } txn =
        {
            I2C::Transaction(0x40, ARRAYLEN(txn.transfers)),
            {
                I2C::Transaction::Transfer(I2C::Transaction::Transfer::TYPE_TX, sizeof(cmd), &cmd),
                I2C::Transaction::Transfer(I2C::Transaction::Transfer::TYPE_RX, sizeof(type), &type),
            }
        };
        I2C::Result result = HYGRO_I2C_BUS.txn(&txn.info);
        // Check if the chip type matches the expected value
        sensor->present = result == I2C::RESULT_OK && (type & 0xfe) == 20;
        if (!sensor->present) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x2170;
        info->info.formatVersion = 0;
    }


    void HumiditySensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Cache enabled channels (before sensor configuration is flushed from data buffer)
        enableHum = info->data[1].u8[27] & 1;
        enableTemp = (info->data[1].u8[27] >> 1) & 1;
        // Calculate total sample size
        info->info.recordSize = (enableHum + enableTemp) * 16;
    }


    void HumiditySensor::start(Sensor* sensor, int time)
    {
        if (!sensor->interval || (!enableHum && !enableTemp)) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        uint8_t reg = info->data[1].u8[0] & 0xf;
        HYGRO_I2C_BUS.writeRegs(0x40, 0x51, &reg, sizeof(reg));  // Write heater control register
        reg = ((info->data[1].u8[0] << 1) & 0x80) | ((info->data[1].u8[0] >> 2) & 4) | ((info->data[1].u8[0] >> 5) & 1);
        HYGRO_I2C_BUS.writeRegs(0x40, 0xe6, &reg, sizeof(reg));  // Write user register
        sendCmd(enableHum ? 0xf5 : 0xf3);  // Start capturing of first sample (split transaction)
    }


    void HumiditySensor::stop(Sensor* sensor)
    {
        collectResult();  // Discard result of previously initiated split transaction
        uint8_t reg = 0;
        HYGRO_I2C_BUS.writeRegs(0x40, 0xe6, &reg, sizeof(reg));  // Clear user register
    }


    void HumiditySensor::capture(Sensor* sensor)
    {
        // Collect first measurement from previously initiated split transaction.
        // This is humidity if that is enabled, otherwise temperature.
        uint16_t result = collectResult();
        SensorTask::writeMeasurement(result);
        if (enableTemp && enableHum)
        {
            // If both temperature and humidity recording is requested,
            // read out the temperature value (captured during humidity measurement) as well
            HYGRO_I2C_BUS.readRegs(0x40, 0xe0, &result, sizeof(result));
            SensorTask::writeMeasurement(result);
        }
        sendCmd(enableHum ? 0xf5 : 0xf3);  // Start capturing of next sample (split transaction)
        // Schedule collection of next sample
        sensor->reschedule(&sensor->captureTask);
    }
}
