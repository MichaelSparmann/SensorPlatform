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


#ifndef STM32_RCC_OPTIMIZE
#define STM32_RCC_OPTIMIZE
#endif


namespace STM32
{
    class __attribute__((packed,aligned(4))) RCC final
    {
    public:
        enum Oscillator
        {
            HSI = 0,
            HSE = 1,
            LSI = 2,
            LSE = 3,
            HSI48 = 4,
        };
        
        enum SysClockSource
        {
            SysClockHSI = 0,
            SysClockHSE = 1,
            SysClockPLL = 2,
            SysClockHSI48 = 3,
        };
        
        enum PLLClockSource
        {
            PLLClockHSIDiv2 = 0,
            PLLClockHSI = 1,
            PLLClockHSE = 2,
            PLLClockHSI48 = 3,
        };
        
        static void init();
        static void initClocks();
        static bool setClockGate(int gate, bool on);
        static int getSysClockFrequency();
        static int getAHBClockFrequency();
        static int getAPBClockFrequency();
#ifdef STM32_RCC_DYNAMIC
        static void setSysClockSource(SysClockSource source);
        static void setAHBDivider(int divider);
        static void setAPBDivider(APBBus bus, int divider);
        static void setPLLClockSource(PLLClockSource source, int divider);
        static void configurePLL(bool on, int factor);
        static void configureOscillator(Oscillator osc, bool on, bool bypass);
#endif
    };
}

