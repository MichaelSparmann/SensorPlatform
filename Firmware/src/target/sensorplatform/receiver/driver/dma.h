#pragma once

// SensorPlatform Base Station DMA Driver (STM32F205)
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
#include "soc/stm32/dma_regs.h"


namespace DMA
{
    enum Direction
    {
        DIR_P2M = 0,  // Peripheral to Memory
        DIR_M2P = 1,  // Memory to Peripheral
        DIR_M2M = 2,  // Memory to Memory
    };

    enum TransferSize
    {
        TS_8BIT = 0,
        TS_16BIT = 1,
        TS_32BIT = 2,
    };

    union __attribute__((packed,aligned(4))) Config
    {
        uint32_t d32;
        struct STM32_DMA_STREAM_REG_TYPE::CR::b b;
        constexpr Config(uint32_t channel, uint32_t priority, Direction direction, TransferSize memSize, bool memIncr,
                         TransferSize periphSize, bool periphIncr, bool irqEnable)
            : b{true, false, false, false, irqEnable, false, direction, false, periphIncr, memIncr,
                periphSize, memSize, false, priority, false, 0, false, 0, 0, channel} {}
    };

    extern void init();
    extern void setPeripheralAddr(volatile STM32_DMA_STREAM_REG_TYPE* regs, volatile void* addr);
    extern void setFIFOConfig(volatile STM32_DMA_STREAM_REG_TYPE* regs, bool direct, int threshold);
    extern void startTransferFromPri0(volatile STM32_DMA_STREAM_REG_TYPE* regs, Config config, void* memAddr, size_t len);
    extern void startTransferWithLock(volatile STM32_DMA_STREAM_REG_TYPE* regs, Config config, void* memAddr, size_t len);
    extern void cancelTransferFromPri0(volatile STM32_DMA_STREAM_REG_TYPE* regs, int controller, int stream);
    extern void cancelTransferWithLock(volatile STM32_DMA_STREAM_REG_TYPE* regs, int controller, int stream);
    extern void clearIRQFromPri0(int controller, int stream);
    extern void clearIRQWithLock(int controller, int stream);
}
