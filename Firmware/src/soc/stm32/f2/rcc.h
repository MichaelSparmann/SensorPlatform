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
        enum APBBus
        {
            APB1 = 0,
            APB2 = 1,
        };

        enum Oscillator
        {
            HSI = 0,
            HSE = 1,
            LSI = 2,
            LSE = 3,
        };
        
        enum PLL
        {
            MainPLL = 0,
            PLLI2S = 1,
            PLLSAI = 2,
        };

        enum SysClockSource
        {
            SysClockHSI = 0,
            SysClockHSE = 1,
            SysClockPLL = 2,
        };
        
        enum PLLClockSource
        {
            PLLClockHSI = 0,
            PLLClockHSE = 1,
        };
        
        static void init();
        static bool setClockGate(int gate, bool on);
        static int getPLLOutFrequency();
        static int getSysClockFrequency();
        static int getAHBClockFrequency();
        static int getAPBClockFrequency(APBBus bus);
        static int get48MClockFrequency();
#ifdef STM32_RCC_DYNAMIC
        static void setSysClockSource(SysClockSource source);
        static void setAHBDivider(int divider);
        static void setAPBDivider(APBBus bus, int divider);
        static void setPLLClockSource(PLLClockSource source, int divider);
        static void configurePLL(PLL pll, bool on, int n, int p, int q, int r);
        static void configureOscillator(Oscillator osc, bool on, bool bypass);
#endif
    };
}

