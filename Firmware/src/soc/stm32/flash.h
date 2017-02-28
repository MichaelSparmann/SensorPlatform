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


#ifndef STM32FLASH_OPTIMIZE
#define STM32FLASH_OPTIMIZE
#endif


#ifndef STM32_VOLTAGE
    #error Unknown STM32 operating voltage, cannot determine correct configuration
#endif

#if defined(SOC_STM32F42X)
    #if (STM32_VOLTAGE) < 1700
        #error STM32 operating voltage is too low! Minimum is 1700mV.
    #elif (STM32_VOLTAGE) < 2100
        #define STM32_FLASH_SPEED 20000000
    #elif (STM32_VOLTAGE) < 2400
        #define STM32_FLASH_SPEED 22000000
    #elif (STM32_VOLTAGE) < 2700
        #define STM32_FLASH_SPEED 24000000
    #elif (STM32_VOLTAGE) < 3600
        #define STM32_FLASH_SPEED 30000000
    #else
        #error STM32 operating voltage is too high! Maximum is 3600mV.
    #endif
#elif defined(SOC_STM32F4)
    #if (STM32_VOLTAGE) < 1700
        #error STM32 operating voltage is too low! Minimum is 1700mV.
    #elif (STM32_VOLTAGE) < 2100
        #define STM32_FLASH_SPEED 16000000
    #elif (STM32_VOLTAGE) < 2400
        #define STM32_FLASH_SPEED 18000000
    #elif (STM32_VOLTAGE) < 2700
        #define STM32_FLASH_SPEED 24000000
    #elif (STM32_VOLTAGE) < 3600
        #define STM32_FLASH_SPEED 30000000
    #else
        #error STM32 operating voltage is too high! Maximum is 3600mV.
    #endif
#elif defined(SOC_STM32F2)
    #if (STM32_VOLTAGE) < 1700
        #error STM32 operating voltage is too low! Minimum is 1700mV.
    #elif (STM32_VOLTAGE) < 2100
        #define STM32_FLASH_SPEED 16000000
    #elif (STM32_VOLTAGE) < 2400
        #define STM32_FLASH_SPEED 18000000
    #elif (STM32_VOLTAGE) < 2700
        #define STM32_FLASH_SPEED 24000000
    #elif (STM32_VOLTAGE) < 3600
        #define STM32_FLASH_SPEED 30000000
    #else
        #error STM32 operating voltage is too high! Maximum is 3600mV.
    #endif
#elif defined(SOC_STM32F1)
    #if (STM32_VOLTAGE) < 2000
        #error STM32 operating voltage is too low! Minimum is 2000mV.
    #elif (STM32_VOLTAGE) < 3600
        #define STM32_FLASH_SPEED 24000000
    #else
        #error STM32 operating voltage is too high! Maximum is 3600mV.
    #endif
#elif defined(SOC_STM32F072)
    #if (STM32_VOLTAGE) < 2000
        #error STM32 operating voltage is too low! Minimum is 2000mV.
    #elif (STM32_VOLTAGE) < 3600
        #define STM32_FLASH_SPEED 24000000
    #else
        #error STM32 operating voltage is too high! Maximum is 3600mV.
    #endif
#elif defined(SOC_STM32F0)
    #if (STM32_VOLTAGE) < 2400
        #error STM32 operating voltage is too low! Minimum is 2400mV.
    #elif (STM32_VOLTAGE) < 3600
        #define STM32_FLASH_SPEED 24000000
    #else
        #error STM32 operating voltage is too high! Maximum is 3600mV.
    #endif
#else
    #error Unknown SOC architecture, cannot determine correct configuration
#endif

namespace STM32
{
    class __attribute__((packed,aligned(4))) FLASH final
    {
    public:
        static void init();
    };
}
