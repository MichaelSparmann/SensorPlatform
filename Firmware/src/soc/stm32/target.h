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


#define SOC_STM32
#define CACHEALIGN_BITS 2
#define DMAALIGN_BITS 2

#ifdef STM32_CAN_BOOT_FROM_RAM
#ifndef RUNNING_FROM_RAM
#ifndef RUNNING_FROM_FLASH
#error Please define either RUNNING_FROM_RAM or RUNNING_FROM_FLASH!
#endif
#endif

#ifdef RUNNING_FROM_RAM
#ifdef RUNNING_FROM_FLASH
#error Both RUNNING_FROM_RAM and RUNNING_FROM_FLASH are defined!?
#endif
#endif
#else
#ifdef RUNNING_FROM_RAM
#error This device cannot boot from RAM!
#endif
#define RUNNING_FROM_FLASH
#endif

#ifndef STM32_AHB_CLOCK
    #define STM32_AHB_CLOCK STM32_MAX_AHB_CLOCK
#endif

#ifndef STM32_APB1_CLOCK
    #define STM32_APB1_CLOCK STM32_MAX_APB1_CLOCK
#endif

#ifndef STM32_APB2_CLOCK
    #define STM32_APB2_CLOCK STM32_MAX_APB2_CLOCK
#endif

#ifndef STM32_ADC_CLOCK
    #ifdef STM32_MAX_ADC_CLOCK
        #define STM32_ADC_CLOCK STM32_MAX_ADC_CLOCK
    #endif
#endif

#ifndef STM32_SYS_CLOCK
    #define STM32_SYS_CLOCK STM32_AHB_CLOCK
#endif

#define CORTEXM_FCLK_FREQUENCY_BOOT STM32_AHB_CLOCK
#define CORTEXM_SYSTICK_FREQUENCY_BOOT (STM32_AHB_CLOCK >> 3)

#ifndef GPIO_STATIC_CONTROLLER
#define GPIO_STATIC_CONTROLLER_HEADER "soc/stm32/gpio.h"
#define GPIO_STATIC_CONTROLLER STM32::GPIO::Controller
#endif

#ifdef __cplusplus
namespace STM32
{
    extern const uint32_t CPUID[3];
}
#endif

