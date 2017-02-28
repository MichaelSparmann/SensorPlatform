// Sensor node SD card access logic
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
#include "storagetask.h"
#include "cpu/arm/cortexm/cortexutil.h"
#include "sys/util.h"
#include "lib/crc32/crc32.h"
#include "common.h"
#include "irq.h"
#include "sd.h"
#include "radio.h"
#include "sensortask.h"
#include "sys/time.h"
#include "soc/stm32/gpio.h"

namespace StorageTask
{
    // Length of series header (including sensor configuration) in SD card sectors
    static const uint32_t seriesHeaderSectors = sizeof(mainBuf.seriesHeader) / sizeof(*mainBuf.block);
    
    // First data sector number on the SD card
    static const uint32_t firstDataSector = 2 + 2 * seriesHeaderSectors;

    // Various interpretations of the SD card sector buffer
    static union __attribute__((packed,aligned(4)))
    {
        uint8_t u8[512];
        uint32_t u32[128];
        struct __attribute__((packed,aligned(4)))
        {
            uint32_t blockSeq;
            uint32_t reserved[7];
            Page data[17];
            uint32_t crc;
        } recording;
    } xferBuf;
    
    static uint8_t currentConfigIndex;  // Which copy of the node config is currently active
    static uint8_t currentConfigUSN;  // Node config update sequence number (may wrap around)
    static uint8_t seriesHeaderIndex;  // Which copy of the series header is currently active
    static uint8_t seriesHeaderUSN;  // Series header update sequence number (may wrap around)
    static uint32_t cmdArg;  // Argument of a pending command (usually an index)
    static uint32_t cmdSize;  // Argument of a pending command (usually a count)

    State state = State_Init;  // Requested or running operation
    bool configDirty;  // Node config in RAM has been modified since last save
    bool seriesHeaderDirty;  // Series header / sensor config in RAM has been modified since last save
    uint32_t bufferOverflowLost;  // How many measurement data pages (28 bytes) have been lost
                                  // due to the SD card not being able to keep up with writing
    static uint32_t currentBlockSeq;  // Block sequence number within measurement data
                                      // being currently written to the SD card
    static uint8_t currentBlock;  // Read pointer within measurement data buffer (in blocks)


    // Load node configuration from SD card (run from storage task)
    static void loadConfig()
    {
        // Lock out other requests until we're done
        if (state != State_Idle) error(Error_StorageLoadConfigNotIdle);
        state = State_LoadConfig;
        
        uint8_t usn[2];
        bool valid[2];
        // Read the first copy of node configuration and validate it
        SD::read(0, 1, &xferBuf);
        valid[0] = crc32(xferBuf.u8 + 4, sizeof(xferBuf) - 4) == *xferBuf.u32;
        usn[0] = xferBuf.u8[4];
        // Read the second copy of node configuration and validate it
        SD::read(1, 1, &xferBuf);
        valid[1] = crc32(xferBuf.u8 + 4, sizeof(xferBuf) - 4) == *xferBuf.u32;
        usn[1] = xferBuf.u8[4];
        // If the second copy is valid and newer than the first, ignore the first one
        if (valid[1] && usn[1] - usn[0] > 0) valid[0] = false;
        
        if (valid[0])
        {
            // The second copy is bad or the first one is newer. Re-read the first one.
            currentConfigIndex = 0;
            SD::read(0, 1, &xferBuf);
        }
        else if (valid[1])
        {
            // The first copy is bad or the second one is older.
            currentConfigIndex = 1;
        }
        else
        {
            // There is no valid node configuration copy.
            // We don't have vital information such as our node ID, so bail out.
            error(Error_StorageLoadConfigCorrupted);   
        }
        
        // Apply the newest copy of the node configuration
        currentConfigUSN = xferBuf.u8[4];
        memcpy(&config, xferBuf.u8 + 8, sizeof(config));
        config.nodeUniqueId.firmware.version = FIRMWARE_VERSION;
        state = State_Idle;
    }

    // Save node configuration to SD card (run from storage task)
    static void doSaveConfig()
    {
        // Reset dirty flag. Any changes past this point might not end up on the SD card.
        configDirty = false;
        // Switch to the sector that contains the older copy of the node configuration
        currentConfigIndex ^= 1;
        // Copy checksum, new update sequence number and the current config into the sector buffer
        memset(&xferBuf, 0, sizeof(xferBuf));
        xferBuf.u8[4] = ++currentConfigUSN;
        memcpy(xferBuf.u8 + 8, &config, sizeof(config));
        *xferBuf.u32 = crc32(xferBuf.u8 + 4, sizeof(xferBuf) - 4);
        // Write the sector buffer to the SD card
        SD::write(currentConfigIndex, 1, &xferBuf);
        // Signal completion
        state = State_Idle;
        SEV();
    }

    // Save node configuration to SD card (called externally)
    void saveConfig()
    {
        // Check if need to and are able to write the configuration
        if (state != State_Idle) error(Error_StorageSaveConfigNotIdle);
        if (!configDirty) return;
        // Signal request and wake up storage task
        state = State_SaveConfig;
        IRQ::wakeStorageTask();
        // Wait for completion
        while (state != State_Idle) WFE();
    }

    // Load and validate a copy of the series header (run from storage task)
    static bool loadSeriesHeader(int sector)
    {
        // Read and check every sector, bail out of checksum or USN is bad.
        for (uint32_t i = 0; i < seriesHeaderSectors; i++)
        {
            SD::read(sector + i, 1, &xferBuf);
            if (crc32(xferBuf.u8 + 4, sizeof(xferBuf) - 4) != *xferBuf.u32) return false;
            if (i && xferBuf.u8[4] != seriesHeaderUSN) return false;
            // This sector is good, copy it to the global data buffer.
            seriesHeaderUSN = xferBuf.u8[4];
            memcpy(mainBuf.block[i], xferBuf.u8 + sizeof(xferBuf) - sizeof(*mainBuf.block), sizeof(*mainBuf.block));
        }
        // All sectors were good, return success.
        return true;
    }

    // Load the series header (including sensor configuration) from SD card (run from storage task)
    static void doLoadSeriesHeader()
    {
        uint8_t usn[2];
        bool valid[2];
        // Attempt to read first copy, check if it's valid and what its USN is.
        valid[0] = loadSeriesHeader(2);
        usn[0] = seriesHeaderUSN;
        // Attempt to read second copy, check if it's valid and what its USN is.
        valid[1] = loadSeriesHeader(2 + seriesHeaderSectors);
        usn[1] = seriesHeaderUSN;
        // If the second copy is valid and newer than the first, ignore the first one
        if (valid[1] && usn[1] - usn[0] > 0) valid[0] = false;
        
        if (valid[0])
        {
            // The second copy is bad or the first one is newer. Re-read the first one.
            seriesHeaderIndex = 0;
            loadSeriesHeader(2);
        }
        else if (valid[1])
        {
            // The first copy is bad or the second one is older.
            seriesHeaderIndex = 1;
        }
        else
        {
            // There is no valid series header copy, so initialize it with zeros.
            // We don't have vital information such as our node ID, so bail out.
            memset(&mainBuf.seriesHeader, 0, sizeof(mainBuf.seriesHeader));
        }

        // Detect present sensors and validate/update their information/configuration.
        SensorTask::detectSensors();
        // Signal completion
        state = State_Idle;
    }

    // Save series header (including sensor configuration) to SD card (run from storage task)
    static void doSaveSeriesHeader()
    {
        // Reset dirty flag. Any changes past this point might not end up on the SD card.
        seriesHeaderDirty = false;
        // Switch to the sector that contains the older copy of the series header
        seriesHeaderIndex ^= 1;
        // Increment update sequence number
        memset(&xferBuf, 0, sizeof(xferBuf));
        xferBuf.u8[4] = ++seriesHeaderUSN;
        // Calculate checksums and write all series header sectors to the selected copy
        for (uint32_t i = 0; i < seriesHeaderSectors; i++)
        {
            memcpy(xferBuf.u8 + sizeof(xferBuf) - sizeof(*mainBuf.block), mainBuf.block[i], sizeof(*mainBuf.block));
            *xferBuf.u32 = crc32(xferBuf.u8 + 4, sizeof(xferBuf) - 4);
            SD::write(2 + seriesHeaderIndex * seriesHeaderSectors + i, 1, &xferBuf);
        }
        // Signal completion
        state = State_Idle;
        SEV();
    }

    // Save series header (including sensor configuration) to SD card (called externally)
    void saveSeriesHeader()
    {
        // Check if need to and are able to write the series header
        if (state != State_Idle) error(Error_StorageSaveSeriesHeaderNotIdle);
        if (!seriesHeaderDirty) return;
        // Signal request and wake up storage task
        state = State_SaveSeriesHeader;
        IRQ::wakeStorageTask();
        // Wait for completion
        while (state != State_Idle) WFE();
    }

    // Put storage task into measurement recording mode (called externally)
    void startRecording()
    {
        // Check if the storage task is able to accept the request
        if (state != State_Idle) error(Error_StorageStartRecordingNotIdle);
        // Signal request and wake up storage task
        state = State_Recording;
        currentBlockSeq = 0;
        IRQ::wakeStorageTask();
    }

    // Put storage task into firmware upload mode (called externally)
    void startUpload()
    {
        if (state == State_Uploading) return;
        // Check if the storage task is able to accept the request
        if (state != State_Idle) error(Error_StorageStartUploadNotIdle);
        state = State_Uploading;
    }

    // Handle upload of a 28 byte chunk of data (called externally in Uploading state)
    void uploadData(uint8_t index, const void* data)
    {
        // Only allowed in Uploading state
        if (state != State_Uploading) error(Error_StorageUploadDataBadState);
        uint32_t blocksize = sizeof(RF::Packet::Command::WritePage::data);
        uint32_t offset = blocksize * index;
        uint32_t len = MIN(blocksize, sizeof(xferBuf) - offset);
        if (len) memcpy(xferBuf.u8 + offset, data, len);
    }

    // Write sector buffer to SD card (called externally in Uploading state)
    void writeSector(uint32_t sector)
    {
        // Only allowed in Uploading state
        if (state != State_Uploading) error(Error_StorageWriteSectorBadState);
        // Signal request and wake up storage task
        state = State_Writing;
        cmdArg = sector;
        IRQ::wakeStorageTask();
        // Wait for completion
        while (state != State_Uploading) WFE();
    }

    // End upload session without actually upgrading firmware (called externally)
    void endUpload()
    {
        if (state == State_Idle) return;
        if (state != State_Uploading) error(Error_StorageEndUploadBadState);
        state = State_Idle;
    }

    // Perform firmware upgrade (run from sensor task)
    static void doUpgradeFirmware()
    {
        // Declare upgrade code entry point (inside sector buffer, uploaded via radio)
        void (*upgrade)(uint32_t page, uint32_t size, uint32_t crc) = (typeof(upgrade))(xferBuf.u8 + 1);
        // Put sensor task into upgrading state and wake it up.
        // (The sensor task, unlike this task, has a hardware timer and can sleep.)
        SensorTask::state = SensorTask::State_Upgrading;
        IRQ::wakeSensorTask();
        // Return control to DPC level (allows response packet to be sent)
        // The sensor task will wake us up 100ms later to initiate the actual upgrade.
        yield();
        // Lock out any IRQs
        enter_critical_section();
        // Initiate firmware upgrade. SD::prepareUpgrade will put the SD card SPI interface into an
        // accessible state and calculate the correct address to be sent to the card. That address,
        // along with the firmware size (in sectors) and the expected CRC, is then passed to the
        // upgrader code in the sector buffer, which is finally invoked.
        upgrade(SD::prepareUpgrade(firstDataSector), cmdSize, cmdArg);
    }

    // Initiate firmware upgrade (called externally in Uploading state)
    void upgradeFirmware(uint32_t size, uint32_t crc)
    {
        // Only allowed in Uploading state
        if (state != State_Uploading) error(Error_StorageUpgradeFirmwareBadState);
        // Signal request and wake up storage task
        state = State_Upgrading;
        cmdSize = size;
        cmdArg = crc;
        IRQ::wakeStorageTask();
        // Return to send response, don't wait for the upgrade to actually happen (it reboots).
    }

    // Move to next measurement data buffer block (run from storage task in Recording state)
    static void nextBlock(bool success)
    {
        // Account for data loss (if any)
        if (!success) bufferOverflowLost += ARRAYLEN(*mainBuf.block);
        // Increment data buffer pointer, wrapping around if necessary
        if (++currentBlock >= ARRAYLEN(mainBuf.block)) currentBlock = 0;
        // Increment sequence number to be written to the SD card
        currentBlockSeq++;
    }

    // Measurement data recording loop (run from storage task in Recording state)
    static void doRecording()
    {
        // Calculate number of SD card data sectors (recording will stop if all are used)
        uint32_t space = SD::pageCount - firstDataSector;
        // Prepare SD card for write operation (card firmware may perform pre-erase)
        SD::startWrite(firstDataSector, space);
        // Zero unused sector header space
        memset(xferBuf.recording.reserved, 0, sizeof(xferBuf.recording.reserved));
        while (true)
        {
            if (mainBufSeq[currentBlock] < currentBlockSeq)
            {
                // No untransmitted data available. If measurement has been stopped, return.
                if (state != State_Recording) break;
                // Otherwise yield control to lower-priority code until we're woken up again.
                else yield();
            }
            if (!space || !mainBufValid[currentBlock] || mainBufSeq[currentBlock] != currentBlockSeq)
            {
                // A measurement data buffer overflow occurred and we lost the block
                // that we currently want to write to the SD card. Skip it.
                nextBlock(false);
                continue;
            }
            // Grab a copy of the data to be sent
            memcpy(xferBuf.recording.data, mainBuf.block[currentBlock], sizeof(*mainBuf.block));
            if (!mainBufValid[currentBlock] || mainBufSeq[currentBlock] != currentBlockSeq)
            {
                // The buffer overflowed while we were copying the block.
                // It was probably partially overwritten. Skip it.
                nextBlock(false);
                continue;
            }
            // Set header fields and write the block to the SD card.
            xferBuf.recording.blockSeq = currentBlockSeq;
            xferBuf.recording.crc = crc32(xferBuf.u8, sizeof(xferBuf) - 4);
            SD::writeSector(xferBuf.u8);
            // This block was written successfully, move on to the next one.
            space--;
            nextBlock(true);
        }
        // Leave SD card write mode.
        // Any data sectors that were not actually written may contain garbage (usually zero) data.
        SD::endWrite();
    }

    // Storage task entry point (initially in Init state)
    static void run()
    {
        // Configure data polling interval (TODO: Make this adaptive)
        Radio::noDataResponse.pollInFrames = 16;
        // Set state to idle to avoid loadConfig bailing out.
        // We can't actually be interrupted before loadConfig will set the state to LoadConfig.
        state = State_Idle;
        loadConfig();
        // Configuration was loaded successfully, trigger loading of series header
        // (will happen in the main loop as if it was triggered externally).
        state = State_LoadSeriesHeader;

        // Storage task main loop
        while (true)
        {
            switch (state)
            {
            case State_SaveConfig:
                doSaveConfig();
                break;

            case State_LoadSeriesHeader:
                doLoadSeriesHeader();
                break;

            case State_SaveSeriesHeader:
                doSaveSeriesHeader();
                break;

            case State_Recording:
                doRecording();
                break;

            case State_Writing:
                SD::write(firstDataSector + cmdArg, 1, &xferBuf);
                // Signal completion
                state = State_Uploading;
                SEV();
                break;

            case State_Upgrading:
                doUpgradeFirmware();
                break;

            case State_Uploading:
            case State_Idle:
                yield();
                break;

            default:
                error(Error_StorageMainLoopInvalidState);
            }
        }
    }

    // Storage task stack. Contains any stack frames of the storage task
    // plus those layered on top by radio IRQ handling.
    static uint32_t __attribute__((section(".stack"))) taskStack[128];
    // Current storage task stack pointer value (if the storage task is not currently running)
    // or stack pointer to return to when yielding (if the storage task is running)
    void* __attribute__((used)) taskStackPtr = taskStack + ARRAYLEN(taskStack) - 9;

    // Switch back and forth between stacks and contexts.
    // Both entry and exit point for the IRQ context that the storage task runs in.
    __attribute__((naked,noinline)) void yield()
    {
        // refers to the taskStackPtr variable above (mangled C++ name)
        CONTEXTSWITCH("_ZN11StorageTask12taskStackPtrE");
    }

    void init()
    {
        SD::init();
        taskStack[ARRAYLEN(taskStack) - 1] = (uint32_t)run;  // Entry point
        IRQ::wakeStorageTask();
    }
}
