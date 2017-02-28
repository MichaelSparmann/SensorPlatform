// Generic Microcontroller Firmware Platform
// Copyright (C) 2017 Michael Sparmann
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
#include "lib/crc32/crc32.h"
#include "soc/stm32/crc32_regs.h"
#include "interface/clockgate/clockgate.h"
#include "sys/util.h"


uint32_t crc32(const void* buf, uint32_t len)
{
    uint32_t* data = (uint32_t*)buf;
    clockgate_enable(STM32_CRC_CLOCKGATE, true);
    union STM32_CRC_REG_TYPE::CR CR = { 0 };
    CR.b.RESET = true;
    STM32_CRC_REGS.CR.d32 = CR.d32;
    while (len >= 1)
    {
        STM32_CRC_REGS.DR = *data++;
        len -= 4;
    }
    uint32_t crc = STM32_CRC_REGS.DR;
    clockgate_enable(STM32_CRC_CLOCKGATE, false);
    return crc;
}
