#pragma once

// Sensor node global variables and functions
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
#include "sys/util.h"
#include "../common/protocol/rfproto.h"


// Fatal error codes: (All of these indicate software bugs or hardware issues)
enum ErrorCode
{
    Error_None = 0,
    Error_CPUFaultNMI,  // CPU non-maskable interrupt
    Error_CPUFaultHard,  // CPU hard fault
    Error_CPUFaultMMU,  // CPU memory management fault
    Error_CPUFaultBus,  // CPU bus fault
    Error_CPUFaultUsage,  // CPU usage fault
    Error_SDCMD0,  // SD card GO_IDLE_STATE failed
    Error_SDInit,  // SD card initialization failed using both command set versions
    Error_SDGetCapacity,  // SD card capacity inquiry failed (first communication at high speed)
    Error_SDReadOutOfBounds,  // SD card read requested with an out of bounds sector number/size
    Error_SDReadSingleCmd,  // SD card rejected READ_SINGLE_BLOCK command
    Error_SDReadSingleXfer,  // SD card data transfer after READ_SINGLE_BLOCK command failed
    Error_SDReadMultipleCmd,  // SD card rejected READ_MULTIPLE_BLOCK command
    Error_SDReadMultipleXfer,  // SD card data transfer after READ_MULTIPLE_BLOCK command failed
    Error_SDReadMultipleEnd,  // SD card STOP_TRANSMISSION command after READ_MULTIPLE_BLOCK failed
    Error_SDWriteOutOfBounds,  // SD card write requested with an out of bounds sector number/size
    Error_SDWriteSingleCmd,  // SD card rejected WRITE_BLOCK command
    Error_SDWriteSingleXfer,  // SD card data transfer after WRITE_BLOCK command failed
    Error_SDWritePreErase,  // SD card rejected SET_WR_BLOCK_ERASE_COUNT command
    Error_SDWriteMultipleCmd,  // SD card rejected WRITE_MULTIPLE_BLOCK command
    Error_SDWriteMultipleXfer,  // SD card data transfer after WRITE_MULTIPLE_BLOCK command failed
    Error_SDWriteMultipleEnd,  // SD card STOP_TRANSMISSION token after WRITE_MULTIPLE_BLOCK failed
    Error_SDWriteStatus,  // SD card reported error in response to SEND_STATUS command after write
    Error_SDEraseOutOfBounds,  // SD card erase requested with an out of bounds sector number/size
    Error_SDEraseSetStart,  // SD card rejected ERASE_WR_BLK_START command
    Error_SDEraseSetEnd,  // SD card rejected ERASE_WR_BLK_END command
    Error_SDEraseCmd,  // SD card rejected ERASE command
    Error_SDEraseEnd,  // SD card ERASE command timed out
    Error_RadioEnterBeaconModeDMACollision,  // DMA in progress while switching to beacon mode
    Error_RadioPTXDMACollision,  // DMA in progress while switching to PTX mode
    Error_RadioPrepareNextTXDMACollision,  // DMA in progress while preparing next TX packet
    Error_RadioPRXDMACollision,  // DMA in progress while switching to PRX mode
    Error_RadioDMACompletionBadState,  // Unexpected state machine state upon DMA completion
    Error_StorageMainLoopInvalidState,  // Unexpected state machine state in storage task main loop
    Error_StorageLoadConfigNotIdle,  // Node configuration load requested but storage task is busy
    Error_StorageLoadConfigCorrupted,  // All node configuration copies are corrupted
    Error_StorageSaveConfigNotIdle,  // Node configuration save requested but storage task is busy
    Error_StorageSaveSeriesHeaderNotIdle,  // Series header save requested but storage task is busy
    Error_StorageStartRecordingNotIdle,  // Data recording requested but storage task is busy
    Error_StorageStartUploadNotIdle,  // Switch to upload mode requested but storage task is busy
    Error_StorageUploadDataBadState,  // Data upload requested but storage task not in upload mode
    Error_StorageWriteSectorBadState,  // Data write requested but storage task not in upload mode
    Error_StorageEndUploadBadState,  // End of upload requested but storage task not in upload mode
    Error_StorageUpgradeFirmwareBadState,  // Upgrade requested but storage task not in upload mode
    Error_SensorMainLoopInvalidState,  // Unexpected state machine state in sensor task main loop
    Error_SensorDetectNotIdle,  // Sensor detection requested but sensor task is busy
    Error_SensorStartMeasurementNotIdle,  // Measurement requested but sensor task is busy
};

// Sensor IDs within the sensor node
enum SensorId
{
    SensorId_Dummy = -1,  // This makes the first entry from sensor_defs.h get ID 0
#define DEFINE_SENSOR(name, obj) SensorId_ ## name,
#include "sensor_defs.h"
#undef DEFINE_SENSOR
};

// Global node configuration information
union __attribute__((packed,aligned(4))) Config
{
    uint8_t u8[504];
    uint32_t u32[126];
    struct __attribute__((packed,aligned(4)))
    {
        RF::NodeIdentification nodeUniqueId;  // Globally unique node hardware and software ID
    };
};

// Measurement data stream page contents
union __attribute__((packed,aligned(4))) Page
{
    uint8_t u8[28];
    uint16_t u16[14];
    uint32_t u32[7];
    struct __attribute__((packed,aligned(4))) SensorInfo  // First config data page of each sensor
    {
        // Sensor hardware identification:
        uint32_t vendor;  // Sensor type vendor ID
        uint32_t product;  // Sensor type product ID
        uint32_t serial;  // Sensor serial number (if available, else zero)
        // Recording data format identification:
        uint32_t formatVendor;  // Recording data format vendor ID
        uint16_t formatType;  // Recording data format type ID
        uint8_t formatVersion;  // Recording data format version
        uint8_t recordSize;  // Bits per sample (may vary for the same formatType)
        // Measurement schedule:
        uint32_t scheduleOffset;  // Offset from beginning of series to first sample (microseconds)
        uint32_t scheduleInterval;  // Time between samples (microseconds)
    } sensorInfo;
};

// Measurement series header
struct __attribute__((packed,aligned(4))) SeriesHeader
{
    union __attribute__((packed,aligned(4))) SeriesInfo  // Series attributes
    {
        Page page[16];
        struct __attribute__((packed,aligned(4)))
        {
            RF::HwUniqueId hwId;  // Globally unique hardware ID of the recording node
            uint32_t localTime;  // Begin microsecond time on recording node (32 bits)
            uint32_t globalTime;  // Begin microsecond time on base station (28 bits)
            uint64_t unixTime;  // Begin unix timestamp on client PC (in microseconds, 64 bits)
            // The rest of this structure is ignored by the sensor node firmware
            // and written via radio commands by the client software. It is omitted here.
        };
    } info;
    union __attribute__((packed,aligned(4))) SensorInfo  // Sensor configuration for each sensor
    {
        Page page[4];
        struct __attribute__((packed,aligned(4)))
        {
            Page::SensorInfo info;  // The first page always contains the fixed format shown above
            Page data[3];  // The remaining 3 pages contain sensor-specific information
        };
    } sensor[64];
};

// Main measurement data and sensor configuration buffer.
// While not measuring, this contains the series header (which includes the current configuration
// of all sensors). During measurement this is used as measurement data stream ring buffer.
union __attribute__((packed,aligned(4))) MainBuf
{
    Page block[MAINBUF_BLOCK_COUNT][17];
    Page page[MAINBUF_BLOCK_COUNT * 17];
    SeriesHeader seriesHeader;
};


// See common.cpp for these:
extern Config config;
extern MainBuf mainBuf;
extern uint32_t mainBufSeq[ARRAYLEN(mainBuf.block)];
extern bool mainBufValid[ARRAYLEN(mainBuf.block)];

extern void __attribute__((noreturn)) error(ErrorCode code);
