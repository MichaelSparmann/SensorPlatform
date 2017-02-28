#pragma once

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


namespace StorageTask
{
    enum State
    {
        State_Idle = 0,  // Sleeping
        State_Init,  // Initial state, blocks other requests before config is loaded initially
        State_LoadConfig,  // Loading node configuration from SD card
        State_SaveConfig,  // Writing node configuration to SD card
        State_LoadSeriesHeader,  // Loading series header (including sensor config) from SD card
        State_SaveSeriesHeader,  // Writing series header (including sensor config) to SD card
        State_Recording,  // Measurement recording in progress
        State_Download,  // Recorded data download (via radio) in progress (not implemented yet)
        State_Uploading,  // Sector buffer locked for SD card access via radio commands
        State_Writing,  // Sector buffer is being written to SD card (from Uploading state)
        State_Upgrading,  // Firmware upgrade triggered, will reboot into the updater
    };

    extern State state;
    extern bool configDirty;
    extern bool seriesHeaderDirty;
    extern uint32_t bufferOverflowLost;

    extern void init();
    extern void saveConfig();
    extern void saveSeriesHeader();
    extern void startRecording();
    extern void startUpload();
    extern void uploadData(uint8_t index, const void* data);
    extern void writeSector(uint32_t sector);
    extern void endUpload();
    extern void upgradeFirmware(uint32_t size, uint32_t crc);
    extern void yield() asm(STORAGETASK_VECTOR);
}
