#pragma once

// Sensor node SD card driver
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


namespace SD
{
    extern uint32_t pageCount;
    extern bool misoEdge;

    extern void reset();
    extern void read(uint32_t page, uint32_t len, void* buf);
    extern void write(uint32_t page, uint32_t len, void* buf);
    extern void startWrite(uint32_t page, uint32_t len);
    extern void writeSector(void* buf);
    extern void endWrite();
    extern void erase(uint32_t page, uint32_t len);
    extern uint32_t prepareUpgrade(uint32_t page);
    extern void init();
    extern void shutdown(bool beforePowerDown);
    extern void wakeup(bool afterPowerDown);
}
