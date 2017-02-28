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
#include "imu.h"
#include "soc/stm32/gpio.h"
#include "sys/util.h"
#include "sys/time.h"
#include "../common.h"
#include "../radio.h"
#include "../irq.h"
#include "../sensortask.h"


namespace IMU
{
    const SensorType accelSensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b1100000000000000000000000001,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x43415092,
        AccelSensor::init, AccelSensor::verify, AccelSensor::start, AccelSensor::stop,
    };

    const SensorType gyroSensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b1111111110000000000000000001,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x59475092,
        GyroSensor::init, GyroSensor::verify, GyroSensor::start, GyroSensor::stop,
    };

    const SensorType magSensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b0000000000000000000000000001,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x474d5092,
        MagSensor::init, MagSensor::verify, MagSensor::start, MagSensor::stop,
    };

    const SensorType tempSensorType
    {
        {
            0b0000000000000000000011111111,
            0b0000000000000000000000000000,
            0b0000000000000000000000000000,
            0b0000000000000000000000000000,
        },
        0x53414149, 0x4d545092,
        TempSensor::init, TempSensor::verify, TempSensor::start, TempSensor::stop,
    };

    AccelSensor accelSensor(SensorId_IMUAccel);
    GyroSensor gyroSensor(SensorId_IMUGyro);
    MagSensor magSensor(SensorId_IMUMag);
    TempSensor tempSensor(SensorId_IMUTemp);
    bool present;  // Whether the IMU chip is present at all
    bool magPresent;  // Whether the magnetometer is present (MPU9250 variant)
    int bootedAt;  // Time when the IMU chip is expected to accept for communication after powerup
    uint8_t reg108;  // Power control register value, affects both gyroscope and accelerometer
    uint8_t i2cEnable;  // I2C slave 4 configuration (for magnetometer access)


    // Read measurement data at high SPI bus clock speed (not allowed for other registers)
    static void readFast(void* buf, int len)
    {
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER_FAST, buf, buf, len);
    }

    // Read a register from the IMU chip
    static uint8_t readReg(uint8_t reg)
    {
        uint8_t msg[] = {(uint8_t)(0x80 | reg), 0xff};
        // Overwrite request with response in-place
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg, msg, sizeof(msg));
        return msg[1];
    }

    // Write a register value to the IMU chip
    static void writeReg(uint8_t reg, uint8_t value)
    {
        uint8_t msg[] = {reg, value};
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg, NULL, sizeof(msg));
    }

    // Write a register value to the magnetometer (via the IMU's I2C master)
    static void i2cWrite(uint8_t reg, uint8_t value)
    {
        // Write to regs 49-52 (I2C slave 4):
        // 49: 0x0c (Write mode, magnetometer's I2C slave address)
        // 50: Magnetometer target register address
        // 51: Data byte to be written
        // 52: 0x80 (Enable I2C slave 4)
        uint8_t msg[] = {49, 0x0c, reg, value, 0x80};
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg, NULL, sizeof(msg));
        // Sleep for 40 microseconds (I2C communication will take at least that long)
        SensorTask::sleepUntil(read_usec_timer() + 40);
        // Poll for completion (this doesn't yield to lower priority tasks!)
        while (readReg(52) & 0x80);
    }

    // Read a register value from the magnetometer (via the IMU's I2C master)
    static uint8_t i2cRead(uint8_t reg)
    {
        // Write to regs 49-52 (I2C slave 4):
        // 49: 0x8c (Read mode, magnetometer's I2C slave address)
        // 50: Magnetometer target register address
        // 51: 0x80 (Dummy data)
        // 52: 0x80 (Enable I2C slave 4)
        uint8_t msg[] = {49, 0x8c, reg, 0x80, 0x80};
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg, NULL, sizeof(msg));
        // Sleep for 40 microseconds (I2C communication will take at least that long)
        SensorTask::sleepUntil(read_usec_timer() + 40);
        // Rewrite message buffer to read from reg 52
        msg[0] = 0x80 | 52;
        // Poll for completion (this doesn't yield to lower priority tasks!)
        // Use first 2 bytes of the buffer as request, and the last 3 bytes as response space.
        // This will read the I2C master's active flag into bit 7 of msg[3]
        // and (once the transfer is finished) the response byte register into msg[4].
        // It will transmit back the first received byte as 3rd request byte, which is acceptable.
        while (msg[3] & 0x80) Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg, msg + 2, sizeof(msg) - 2);
        // Return the response byte
        return msg[4];
    }

    // Put the IMU into sleep mode. Called after sensor powerup as soon as it is ready for commands.
    void powerDown()
    {
        // Check if the IMU chip is present (responds with proper WHOAMI register value)
        present = readReg(117) == 0x71;
        if (present)
        {
            writeReg(106, 0x30);  // Disable IMU's I2C slave, enable internal I2C master
            writeReg(108, 0x3f);  // Power down accelerometers and gyroscopes
            writeReg(107, 0x19);  // Auto-select clock source, power down PTAT and gyro sense paths
            writeReg(36, 0x0d);  // I2C master bus frequency: 400kHz
            magPresent = i2cRead(0) == 0x48;  // Check if magnetometer is present (and responds)
            writeReg(106, 0);  // Disable internal I2C master
            writeReg(107, 0x4f);  // Put IMU chip into sleep mode, stop internal clocks
        }
    }

    void AccelSensor::init(Sensor* sensor, bool first)
    {
        sensor->present = IMU::present;  // This is a core part of the IMU chip
        if (!IMU::present) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x5092;
        info->info.formatVersion = 0;
        // Read calibration data from the sensor
        for (int i = 0; i < 3; i++) info->data[0].u8[i] = readReg(13 + i);
    }


    void AccelSensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Cache which channels are enabled (before sensor configuration is
        // flushed from data buffer) and calculate sample size (in bits)
        AccelSensor* s = (AccelSensor*)sensor;
        uint8_t channels = info->data[1].u8[27];
        s->enableX = (channels >> 2) & 1;
        s->enableY = (channels >> 1) & 1;
        s->enableZ = channels & 1;
        info->info.recordSize = (s->enableX + s->enableY + s->enableZ) * 16;
    }


    void AccelSensor::start(Sensor* sensor, int time)
    {
        // This is the first IMU sensor that will be started (lowest ID).
        // Wake up the chip from sleep mode and set clock source to auto-select.
        writeReg(107, 0x01);
        // Initialize power control register value to "all channels disabled"
        reg108 = 0x3f;
        if (sensor->interval)
        {
            // The accelerometer is planned to be sampled
            SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
            // Enable the requested channels in the power control register value
            uint8_t channels = info->data[1].u8[27];
            reg108 &= ~(channels << 3);
            // Write requested accelerometer configuration to the sensor
            for (int i = 0; i < 2; i++) writeReg(28 + i, info->data[1].u8[i]);
        }
    }


    void AccelSensor::stop(Sensor* sensor)
    {
        // This is the last IMU sensor that will bve shut down (lowest ID).
        writeReg(108, 0x3f);  // Disable all accelerometers and gyroscopes.
        writeReg(107, 0x4f);  // Put the chip into sleep mode and stop internal clocks.
    }


    void AccelSensor::capture(Sensor* sensor)
    {
        AccelSensor* s = (AccelSensor*)sensor;
        // Read accelerometer measurements from IMU chip
        struct __attribute__((packed))
        {
            uint8_t padding;  // Align accelX to a 16-bit boundary
            uint8_t cmd;  // Read registers 59-61
            int16_t accelX;
            int16_t accelY;
            int16_t accelZ;
        } buf = {0, 0x80 | 59, -1, -1, -1};
        readFast(&buf.cmd, sizeof(buf) - sizeof(buf.padding));
        // Record the requested channels
        if (s->enableX) SensorTask::writeMeasurement(buf.accelX);
        if (s->enableY) SensorTask::writeMeasurement(buf.accelY);
        if (s->enableZ) SensorTask::writeMeasurement(buf.accelZ);
        // Schedule capturing of next sample
        s->reschedule(&s->captureTask);
    }


    void GyroSensor::init(Sensor* sensor, bool first)
    {
        sensor->present = IMU::present;  // This is a core part of the IMU chip
        if (!IMU::present) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x5192;
        info->info.formatVersion = 0;
        // Read calibration data from the sensor
        for (int i = 0; i < 3; i++) info->data[0].u8[i] = readReg(i);
    }


    void GyroSensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Cache which channels are enabled (before sensor configuration is
        // flushed from data buffer) and calculate sample size (in bits)
        GyroSensor* s = (GyroSensor*)sensor;
        uint8_t channels = info->data[1].u8[27];
        s->enableX = (channels >> 2) & 1;
        s->enableY = (channels >> 1) & 1;
        s->enableZ = channels & 1;
        info->info.recordSize = (s->enableX + s->enableY + s->enableZ) * 16;
    }


    void GyroSensor::start(Sensor* sensor, int time)
    {
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Write requested gyroscope configuration to the sensor
        for (int i = 0; i < 9; i++) writeReg(19 + i, info->data[1].u8[i]);
        // If the gyroscope is planned to be sampled,
        // enable the requested channels in the power control register value
        if (sensor->interval) reg108 &= ~info->data[1].u8[27];
        // Write the power control register value to the IMU chip (also for accelerometers!)
        writeReg(108, reg108);
    }


    void GyroSensor::stop(Sensor* sensor)
    {
        // Everything is taken care of by AccelSensor::stop
    }


    void GyroSensor::capture(Sensor* sensor)
    {
        GyroSensor* s = (GyroSensor*)sensor;
        // Read gyroscope measurements from IMU chip
        struct __attribute__((packed))
        {
            uint8_t padding;  // Align gyroX to a 16-bit boundary
            uint8_t cmd;  // Read registers 67-69
            int16_t gyroX;
            int16_t gyroY;
            int16_t gyroZ;
        } buf = {0, 0x80 | 67, -1, -1, -1};
        readFast(&buf.cmd, sizeof(buf) - sizeof(buf.padding));
        // Record the requested channels
        if (s->enableX) SensorTask::writeMeasurement(buf.gyroX);
        if (s->enableY) SensorTask::writeMeasurement(buf.gyroY);
        if (s->enableZ) SensorTask::writeMeasurement(buf.gyroZ);
        // Schedule capturing of next sample
        s->reschedule(&s->captureTask);
    }


    void MagSensor::init(Sensor* sensor, bool first)
    {
        sensor->present = magPresent;  // Only present on MPU9250 variant
        if (!magPresent) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x5292;
        info->info.formatVersion = 0;
        writeReg(107, 0x19);  // Wake up IMU chip from sleep and set clock source to auto-select
        writeReg(106, 0x20);  // Enable internal I2C master
        i2cWrite(0x0c, 0x40);  // Enter magnetometer self-test mode
        i2cWrite(0x0a, 0x18);  // 16 bit output, initiate self-test
        while (!(i2cRead(0x02) & 0x01));  // Wait for self-test to complete
        // Read self-test/calibration results from the sensor
        for (int i = 0; i < 7; i++) info->data[0].u8[i] = i2cRead(0x03 + i);
        i2cWrite(0x0c, 0);  // Leave magnetometer self-test mode
        i2cWrite(0x0a, 0x0f);  // Enter magnetometer fuse ROM access mode
        // Read sensitivity adjustment values from the sensor
        for (int i = 0; i < 3; i++) info->data[0].u8[7 + i] = i2cRead(0x10 + i);
        i2cWrite(0x0a, 0);  // Leave magnetometer fuse ROM access mode (enter power-down mode)
        writeReg(106, 0);  // Disable internal I2C master
        writeReg(107, 0x4f);  // Put IMU chip into sleep mode and stop internal clocks.
    }


    void MagSensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        // Cache which channels are enabled (before sensor configuration is
        // flushed from data buffer) and calculate sample size (in bits)
        MagSensor* s = (MagSensor*)sensor;
        uint8_t channels = info->data[1].u8[27];
        s->enableX = (channels >> 2) & 1;
        s->enableY = (channels >> 1) & 1;
        s->enableZ = channels & 1;
        info->info.recordSize = (s->enableX + s->enableY + s->enableZ) * 16;
    }


    void MagSensor::start(Sensor* sensor, int time)
    {
        // No need to do anything if no magnetometer sampling is requested
        if (!sensor->interval) return;
        writeReg(106, 0x20);  // Enable internal I2C master
        // Figure out gyro sampling rate (in kHz)
        SeriesHeader::SensorInfo* gyroInfo = gyroSensor.getInfoPtr();
        uint8_t dlpfCfg = gyroInfo->data[1].u8[7] & 0x07;
        int sampleRate;
        if (gyroInfo->data[1].u8[8] & 0x03) sampleRate = 32;
        else if (dlpfCfg == 0 || dlpfCfg  == 7) sampleRate = 8;
        else sampleRate = 1;
        sampleRate /= 1 + gyroInfo->data[1].u8[6];
        // I2C slave 4 configuration: (used to trigger magnetometer measurements)
        // reg 49: 0x0c (Write mode, magnetometer's I2C slave address)
        // reg 50: 0x0a (Magnetometer target register address: CNTL1)
        // reg 51: 0x11 (Data byte to be written: Trigger single 16-bit measurement)
        // reg 52: Enable I2C slave 4, set I2C sampling divider for a maximum of 1kHz sampling
        i2cEnable = 0x80 | MAX(0, sampleRate - 1);
        const uint8_t msg[] = {49, 0x0c, 0x0a, 0x11, i2cEnable};
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg, NULL, sizeof(msg));
        writeReg(103, 0x81);  // Double buffer I2C sensor data, enable sampling divider for slave 0
        // I2C slave 0 configuration: (used to collect magnetometer measurement results)
        // reg 37: 0x8c (Read mode, agnetometer's I2C slave address)
        // reg 38: 0x03 (Magnetometer target register address: HXL)
        // reg 39: Enable I2C slave 0, no byte swapping or offset, transfer 7 bytes
        const uint8_t msg2[] = {37, 0x8c, 0x03, 0x87};
        Radio::sharedSPITransfer(PIN_IMU_NCS, IMU_SPI_PRESCALER, msg2, NULL, sizeof(msg2));
    }


    void MagSensor::stop(Sensor* sensor)
    {
        writeReg(39, 0);  // Disable I2C slave 0
        writeReg(106, 0);  // Disable internal I2C master
    }


    void MagSensor::capture(Sensor* sensor)
    {
        GyroSensor* s = (GyroSensor*)sensor;
        struct __attribute__((packed))
        {
            uint8_t padding;  // Align magX to a 16-bit boundary
            uint8_t cmd;  // Read registers 73-78
            int16_t magX;
            int16_t magY;
            int16_t magZ;
        } buf = {0, 0x80 | 73, -1, -1, -1};
        // Trigger sampling of next measurement
        writeReg(52, i2cEnable);
        // Read accelerometer measurements from IMU chip
        readFast(&buf.cmd, sizeof(buf) - sizeof(buf.padding));
        // Record the requested channels
        if (s->enableX) SensorTask::writeMeasurement(buf.magX);
        if (s->enableY) SensorTask::writeMeasurement(buf.magY);
        if (s->enableZ) SensorTask::writeMeasurement(buf.magZ);
        // Schedule capturing of next sample
        s->reschedule(&s->captureTask);
    }


    void TempSensor::init(Sensor* sensor, bool first)
    {
        sensor->present = IMU::present;  // This is a core part of the IMU chip
        if (!IMU::present) return;
        SeriesHeader::SensorInfo* info = sensor->getInfoPtr();
        // Initialize data format information
        info->info.formatVendor = 0x53414149;
        info->info.formatType = 0x5392;
        info->info.formatVersion = 0;
    }


    void TempSensor::verify(Sensor* sensor, SeriesHeader::SensorInfo* info)
    {
        info->info.recordSize = 16;
    }


    void TempSensor::start(Sensor* sensor, int time)
    {
    }


    void TempSensor::stop(Sensor* sensor)
    {
    }


    void TempSensor::capture(Sensor* sensor)
    {
        struct __attribute__((packed))
        {
            uint8_t padding;  // Align temp to a 16-bit boundary
            uint8_t cmd;  // Read registers 65-66
            int16_t temp;
        } buf = {0, 0x80 | 65, -1};
        // Read accelerometer measurements from IMU chip
        readFast(&buf.cmd, sizeof(buf) - sizeof(buf.padding));
        // Record the measured value
        SensorTask::writeMeasurement(buf.temp);
        // Schedule capturing of next sample
        sensor->reschedule(&sensor->captureTask);
    }
}
