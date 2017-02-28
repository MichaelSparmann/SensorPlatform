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


struct __attribute__((packed)) STM32_EXTI_REG_TYPE
{
    uint32_t IMR;
    uint32_t EMR;
    uint32_t RTSR;
    uint32_t FTSR;
    uint32_t SWIER;
    uint32_t PR;
};

#if defined(SOC_STM32F0) || defined(SOC_STM32F1)
#define STM32_EXTI_REGS (*((volatile STM32_EXTI_REG_TYPE*)0x40010400))
#else
#define STM32_EXTI_REGS (*((volatile STM32_EXTI_REG_TYPE*)0x40013c00))
#endif
