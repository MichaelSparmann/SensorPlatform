#pragma once

// STM32F072 DMA driver
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
#include "soc/stm32/f0/dma_regs.h"


namespace DMA
{
    enum Direction
    {
        DIR_P2M = 0,  // Peripheral to Memory
        DIR_M2P = 1,  // Memory to Peripheral
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
        struct STM32_DMA_STREAM_REG_TYPE::CCR::b b;
        constexpr Config() : d32(0) {}
        constexpr Config(uint32_t priority, Direction direction, bool circular, TransferSize memSize, bool memIncr,
                         TransferSize periphSize, bool periphIncr, bool irqEnable)
            : b{true, irqEnable, false, false, direction, circular, periphIncr, memIncr,
                periphSize, memSize, priority, false} {}
    };

    extern void init();
    extern void setPeripheralAddr(volatile STM32_DMA_STREAM_REG_TYPE* stream, volatile void* addr);
    extern void startTransferFromPri0(volatile STM32_DMA_STREAM_REG_TYPE* stream, Config config, void* memAddr, size_t len);
    extern void startTransferWithLock(volatile STM32_DMA_STREAM_REG_TYPE* stream, Config config, void* memAddr, size_t len);
    extern void cancelTransfer(volatile STM32_DMA_STREAM_REG_TYPE* stream);
    extern uint32_t checkIRQ(int controller, int stream);
    extern void clearIRQFromPri0(int controller, int stream);
    extern void clearIRQWithLock(int controller, int stream);
}
