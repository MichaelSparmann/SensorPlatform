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


#define STM32_SYSCFG_RESETLINE 0
#define STM32_USART6_RESETLINE 5
#define STM32_USART7_RESETLINE 6
#define STM32_USART8_RESETLINE 7
#define STM32_ADC_RESETLINE 9
#define STM32_TIM1_RESETLINE 11
#define STM32_SPI1_RESETLINE 12
#define STM32_USART1_RESETLINE 14
#define STM32_TIM15_RESETLINE 16
#define STM32_TIM16_RESETLINE 17
#define STM32_TIM17_RESETLINE 18
#define STM32_DBGMCU_RESETLINE 22
#define STM32_TIM2_RESETLINE 32
#define STM32_TIM3_RESETLINE 33
#define STM32_TIM6_RESETLINE 36
#define STM32_TIM7_RESETLINE 37
#define STM32_TIM14_RESETLINE 40
#define STM32_WWDG_RESETLINE 43
#define STM32_SPI2_RESETLINE 46
#define STM32_USART2_RESETLINE 49
#define STM32_USART3_RESETLINE 50
#define STM32_USART4_RESETLINE 51
#define STM32_USART5_RESETLINE 52
#define STM32_I2C1_RESETLINE 53
#define STM32_I2C2_RESETLINE 54
#define STM32_USB_RESETLINE 55
#define STM32_CAN_RESETLINE 57
#define STM32_CRS_RESETLINE 59
#define STM32_PWR_RESETLINE 60
#define STM32_DAC_RESETLINE 61
#define STM32_CEC_RESETLINE 62
#define STM32_GPIO_RESETLINE(x) (113 + (x))
#define STM32_GPIOA_RESETLINE STM32_GPIO_RESETLINE(0)
#define STM32_GPIOB_RESETLINE STM32_GPIO_RESETLINE(1)
#define STM32_GPIOC_RESETLINE STM32_GPIO_RESETLINE(2)
#define STM32_GPIOD_RESETLINE STM32_GPIO_RESETLINE(3)
#define STM32_GPIOE_RESETLINE STM32_GPIO_RESETLINE(4)
#define STM32_GPIOF_RESETLINE STM32_GPIO_RESETLINE(5)
#define STM32_TSC_RESETLINE 120
