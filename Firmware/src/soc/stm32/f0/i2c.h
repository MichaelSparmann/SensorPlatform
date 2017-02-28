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


namespace STM32
{
    class __attribute__((packed,aligned(4))) I2C final
    {
    public:
        union __attribute__((packed)) Timing
        {
            uint32_t d32;
            struct __attribute__((packed))
            {
                uint32_t SCLL : 8;
                uint32_t SCLH : 8;
                uint32_t SDADEL : 4;
                uint32_t SCLDEL : 4;
                uint32_t : 4;
                uint32_t PRESC : 4;
            } b;
            constexpr Timing(uint32_t prescaler, uint32_t sclLowHold, uint32_t sclHighHold,
                             uint32_t sdaSetup, uint32_t sdaHold)
                : b{sclLowHold, sclHighHold, sdaHold, sdaSetup, prescaler} {}
        };

        union __attribute__((packed)) Timeout
        {
            uint32_t d32;
            struct __attribute__((packed))
            {
                uint32_t TIMEOUTA : 12;
                uint32_t TIDLE : 1;
                uint32_t : 2;
                uint32_t TIMOUTEN : 1;
                uint32_t TIMEOUTB : 12;
                uint32_t : 3;
                uint32_t TEXTEN : 1;
            } b;
            constexpr Timeout(bool singleBitEnable, uint32_t singleBitTimeout,
                              bool cumulativeEnable, uint32_t cumulativeTimeout)
                : b{singleBitTimeout, 0, singleBitEnable, cumulativeTimeout, cumulativeEnable} {}
        };
    };

}

