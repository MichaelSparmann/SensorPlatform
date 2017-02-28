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


#define SOC_STM32F2
#ifdef STM32_USE_INOFFICIAL
#define FLASH_SIZE 1M
#define SRAM_SIZE 128K
#else
#if defined(SOC_STM32F2XXXB)
#define FLASH_SIZE 128K
#define SRAM_SIZE 64K
#elif defined(SOC_STM32F2XXXC)
#define FLASH_SIZE 256K
#define SRAM_SIZE 96K
#elif defined(SOC_STM32F2XXXE)
#define FLASH_SIZE 512K
#define SRAM_SIZE 128K
#elif defined(SOC_STM32F2XXXF)
#define FLASH_SIZE 512K
#define SRAM_SIZE 128K
#elif defined(SOC_STM32F2XXXG)
#define FLASH_SIZE 1M
#define SRAM_SIZE 128K
#else
#error Unknown STM32F2 variant!
#endif
#endif
#define STM32_CAN_BOOT_FROM_RAM
#define DFU_RAM_SIZE 8K
#define STM32_CPUID_ADDR 0x1fff7a10
#include "soc/stm32/f2/clockgates.h"
#include "soc/stm32/f2/resetlines.h"
#define STM32_GPIO_DEF_FILE "soc/stm32/f2/gpio_defs.h"

#define STM32_HSI_FREQUENCY 16000000
#define STM32_MAX_SYS_CLOCK 120000000
#define STM32_MAX_AHB_CLOCK 120000000
#define STM32_MAX_APB1_CLOCK 30000000
#define STM32_MAX_APB2_CLOCK 60000000
#define STM32_MAX_ADC_CLOCK 30000000
#include "soc/stm32/target.h"

#define CORTEXM_IRQ_DEF_FILE "soc/stm32/f2/irq_defs.h"

#define __NVIC_PRIO_BITS 4
#include "cpu/arm/cortexm/m3/target.h"

