#pragma once

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

struct __attribute__((packed)) STM32_CRC_REG_TYPE
{
    uint32_t DR;
    uint32_t IDR;
    union CR
    {
        uint32_t d32;
        struct __attribute__((packed)) b
        {
            uint32_t RESET : 1;
            uint32_t : 31;
        } b;
    } CR;
};

#define STM32_CRC_REGS (*((volatile STM32_CRC_REG_TYPE*)0x40023000))
