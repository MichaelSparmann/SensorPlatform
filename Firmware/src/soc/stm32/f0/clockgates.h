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


#define STM32_DMA_CLOCKGATE 0
#define STM32_DMA2_CLOCKGATE 1
#define STM32_SRAM_CLOCKGATE 2
#define STM32_FLITF_CLOCKGATE 4
#define STM32_CRC_CLOCKGATE 6
#define STM32_GPIO_CLOCKGATE(x) (17 + (x))
#define STM32_GPIOA_CLOCKGATE STM32_GPIO_CLOCKGATE(0)
#define STM32_GPIOB_CLOCKGATE STM32_GPIO_CLOCKGATE(1)
#define STM32_GPIOC_CLOCKGATE STM32_GPIO_CLOCKGATE(2)
#define STM32_GPIOD_CLOCKGATE STM32_GPIO_CLOCKGATE(3)
#define STM32_GPIOE_CLOCKGATE STM32_GPIO_CLOCKGATE(4)
#define STM32_GPIOF_CLOCKGATE STM32_GPIO_CLOCKGATE(5)
#define STM32_TSC_CLOCKGATE 24
#define STM32_SYSCFG_CLOCKGATE 32
#define STM32_USART6_CLOCKGATE 37
#define STM32_USART7_CLOCKGATE 38
#define STM32_USART8_CLOCKGATE 39
#define STM32_ADC_CLOCKGATE 41
#define STM32_TIM1_CLOCKGATE 43
#define STM32_SPI1_CLOCKGATE 44
#define STM32_USART1_CLOCKGATE 46
#define STM32_TIM15_CLOCKGATE 48
#define STM32_TIM16_CLOCKGATE 49
#define STM32_TIM17_CLOCKGATE 50
#define STM32_DBGMCU_CLOCKGATE 54
#define STM32_TIM2_CLOCKGATE 64
#define STM32_TIM3_CLOCKGATE 65
#define STM32_TIM6_CLOCKGATE 68
#define STM32_TIM7_CLOCKGATE 69
#define STM32_TIM14_CLOCKGATE 72
#define STM32_WWDG_CLOCKGATE 75
#define STM32_SPI2_CLOCKGATE 78
#define STM32_USART2_CLOCKGATE 81
#define STM32_USART3_CLOCKGATE 82
#define STM32_USART4_CLOCKGATE 83
#define STM32_USART5_CLOCKGATE 84
#define STM32_I2C_CLOCKGATE(x) (85 + (x))
#define STM32_I2C1_CLOCKGATE STM32_I2C_CLOCKGATE(0)
#define STM32_I2C2_CLOCKGATE STM32_I2C_CLOCKGATE(1)
#define STM32_USB_CLOCKGATE 87
#define STM32_CAN_CLOCKGATE 89
#define STM32_CRS_CLOCKGATE 91
#define STM32_PWR_CLOCKGATE 92
#define STM32_DAC_CLOCKGATE 93
#define STM32_CEC_CLOCKGATE 94