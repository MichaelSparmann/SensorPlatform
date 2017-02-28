#pragma once

// SensorPlatform Base Station SPI Driver (STM32F205)
// Copyright (C) 2016-2017 Michael Sparmann
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
#include "soc/stm32/spi_regs.h"


#ifndef SPI_OPTIMIZE
#define SPI_OPTIMIZE
#endif


namespace SPI
{
    extern void init(volatile STM32_SPI_REG_TYPE* regs);
    extern uint8_t calcFrequency(int clkgate, int frequency);
    extern void setFrequency(volatile STM32_SPI_REG_TYPE* regs, uint8_t prescaler);
    extern void pushByte(volatile STM32_SPI_REG_TYPE* regs, uint8_t byte);
    extern uint8_t pullByte(volatile STM32_SPI_REG_TYPE* regs);
    extern uint8_t xferByte(volatile STM32_SPI_REG_TYPE* regs, uint8_t byte);
    extern void waitDone(volatile STM32_SPI_REG_TYPE* regs);
    inline volatile void* __attribute__((const)) getDataRegPtr(volatile STM32_SPI_REG_TYPE* regs) { return &regs->DR; }
}
