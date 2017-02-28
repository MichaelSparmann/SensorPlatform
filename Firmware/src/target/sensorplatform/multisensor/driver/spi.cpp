// STM32F072 SPI driver for sensor nodes
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
#include "spi.h"
#include "sys/util.h"
#include "soc/stm32/f0/rcc.h"


namespace SPI
{
    // Initialize an SPI interface
    void SPI_OPTIMIZE init(volatile STM32_SPI_REG_TYPE* regs)
    {
        union STM32_SPI_REG_TYPE::CR1 CR1 = { 0 };
        regs->CR1.d32 = CR1.d32;
        regs->I2SCFGR.d32 = 0;
        union STM32_SPI_REG_TYPE::CR2 CR2 = { 0 };
        CR2.b.FRXTH = true;
        CR2.b.DS = 7;
        CR2.b.TXDMAEN = true;
        CR2.b.RXDMAEN = true;
        regs->CR2.d32 = CR2.d32;
    }

    // Calculate the required clock divider for a given SPI interface and desired bus frequency
    uint8_t SPI_OPTIMIZE calcFrequency(int frequency)
    {
        int clock = STM32::RCC::getAPBClockFrequency();
        // Calculate log_2(ceil(frequency / clock))
        int br = 0;
        for (clock >>= 1; clock > frequency && br < 7; clock >>= 1) br++;
        return br;
    }

    // Program the clock divider of an SPI interface
    void SPI_OPTIMIZE setFrequency(volatile STM32_SPI_REG_TYPE* regs, uint8_t prescaler)
    {
        // Configure: Master mode, software slave select, enable, prescaler
        union STM32_SPI_REG_TYPE::CR1 CR1 = { 0 };
        CR1.b.SSM = true;
        CR1.b.SSI = true;
        CR1.b.MSTR = true;
        CR1.b.SPE = true;
        CR1.b.BR = prescaler;
        regs->CR1.d32 = CR1.d32;
    }

    // Start transmission of a byte
    void SPI_OPTIMIZE pushByte(volatile STM32_SPI_REG_TYPE* regs, uint8_t byte)
    {
        while (!(regs->SR.b.TXE));
        regs->DR = byte;
    }

    // Wait for and get a response byte of a previous transmission
    uint8_t SPI_OPTIMIZE pullByte(volatile STM32_SPI_REG_TYPE* regs)
    {
        while (!(regs->SR.b.RXNE));
        return regs->DR;
    }

    // Bidirectional one-byte transfer
    uint8_t SPI_OPTIMIZE xferByte(volatile STM32_SPI_REG_TYPE* regs, uint8_t byte)
    {
        pushByte(regs, byte);
        return pullByte(regs);
    }

    // Finish transmissions and drain whatever was received
    void SPI_OPTIMIZE waitDone(volatile STM32_SPI_REG_TYPE* regs)
    {
        while (true)
        {
            union STM32_SPI_REG_TYPE::SR SR = { regs->SR.d32 };
            if (!SR.b.BSY && !SR.b.RXNE) break;
            discard(regs->DR);
        }
    }
}
