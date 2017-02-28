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
#include "soc/stm32/flash.h"
#include "soc/stm32/flash_regs.h"
#include "sys/util.h"


#ifdef SOC_STM32F0
#include "soc/stm32/f0/rcc_regs.h"
#endif
#ifdef SOC_STM32F1
#include "soc/stm32/f1/rcc_regs.h"
#endif


#define STM32_FLASH_WAITSTATES (((STM32_AHB_CLOCK) - 1) / (STM32_FLASH_SPEED))
#if STM32_FLASH_WAITSTATES > 15
#error STM32 AHB bus frequency is too high for this operating voltage!
#elif !defined(SOC_STM32F42X) && STM32_FLASH_WAITSTATES > 7
#error STM32 AHB bus frequency is too high for this operating voltage!
#elif defined(SOC_STM32F0) && STM32_FLASH_WAITSTATES > 1
#error STM32 AHB bus frequency is too high for this operating voltage!
#endif


namespace STM32
{

    void FLASH::init()
    {
        // Configure number of waitstates correctly
        union STM32_FLASH_REG_TYPE::ACR ACR = { 0 };
#if defined(SOC_STM32F0) || defined(SOC_STM32F1)
#ifndef STM32_FLASH_DISABLE_PREFETCH
        ACR.b.PRFTBE = true;
#endif
#else
        ACR.b.DCEN = true;
        ACR.b.ICEN = true;
#ifdef STM32_FLASH_PREFETCH
        ACR.b.PRFTEN = true;
#endif
#endif
        ACR.b.LATENCY = STM32_FLASH_WAITSTATES;
        STM32_FLASH_REGS.ACR.d32 = ACR.d32;
    }

}
