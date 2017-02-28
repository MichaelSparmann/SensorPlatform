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


struct __attribute__((packed)) STM32_RNG_REG_TYPE
{
    union CR
    {
        uint32_t d32;
        struct __attribute__((packed)) b
        {
            uint32_t : 2;
            uint32_t RNGEN : 1;
            uint32_t IE : 1;
            uint32_t : 28;
        } b;
    } CR;
    union SR
    {
        uint32_t d32;
        struct __attribute__((packed)) b
        {
            uint32_t DRDY : 1;
            uint32_t CECS : 1;
            uint32_t SECS : 1;
            uint32_t : 2;
            uint32_t CEIS : 1;
            uint32_t SEIS : 1;
            uint32_t : 25;
        } b;
    } SR;
    uint32_t DR;
};

#define STM32_RNG_REGS (*((volatile STM32_RNG_REG_TYPE*)0x50060800))
