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
#include "dma.h"
#include "sys/util.h"
#include "cpu/arm/cortexm/irq.h"
#include "soc/stm32/f0/rcc_regs.h"
#include "interface/irq/irq.h"


namespace DMA
{
#ifdef DMA_ALWAYS_ON
#define onFromPri0() {}
#define offFromPri0() {}
#define onWithLock() {}
#define offWithLock() {}
#else
    // Usage counter (number of active streams)
    static uint8_t users;

    // Enable all clocks required for DMA from a non-preemptable context
    static void onFromPri0()
    {
        // Enable DMA core clock and keep SRAM clock active during CPU sleep
        if (!users++) *STM32_RCC_REGS.CLKGATES.d32 |= (1 << STM32_SRAM_CLOCKGATE) | (1 << STM32_DMA_CLOCKGATE);
    }

    // Decrement usage counter and potentially shut off DMA from a non-preemptable context
    static void offFromPri0()
    {
        // Disable DMA core clock and stop SRAM clock during CPU sleep
        if (!--users) *STM32_RCC_REGS.CLKGATES.d32 &= ~((1 << STM32_SRAM_CLOCKGATE) | (1 << STM32_DMA_CLOCKGATE));
    }

    // Enable all clocks required for DMA with preemption lockout
    static void onWithLock()
    {
        __asm__ volatile("cpsid if");
        if (!users++) *STM32_RCC_REGS.CLKGATES.d32 |= (1 << STM32_SRAM_CLOCKGATE) | (1 << STM32_DMA_CLOCKGATE);
        __asm__ volatile("cpsie if");
    }

    // Decrement usage counter and potentially shut off DMA with preemption lockout
    static void offWithLock()
    {
        __asm__ volatile("cpsid if");
        if (!--users) *STM32_RCC_REGS.CLKGATES.d32 &= ~((1 << STM32_SRAM_CLOCKGATE) | (1 << STM32_DMA_CLOCKGATE));
        __asm__ volatile("cpsie if");
    }
#endif

    // Initialize DMA hardware
    void init()
    {
        // If we don't want power management, we need to turn power on once on init
#ifdef DMA_ALWAYS_ON
        *STM32_RCC_REGS.CLKGATES.d32 |= (1 << STM32_SRAM_CLOCKGATE) | (1 << STM32_DMA_CLOCKGATE);
#endif
    }

    // Configure peripheral-side address of a DMA stream
    void setPeripheralAddr(volatile STM32_DMA_STREAM_REG_TYPE* regs, volatile void* addr)
    {
        onWithLock();
        regs->CPAR = addr;
        offWithLock();
    }

    // Initiate a DMA transfer from a non-preemptable context
    void startTransferFromPri0(volatile STM32_DMA_STREAM_REG_TYPE* regs, Config config, void* memAddr, size_t len)
    {
        onFromPri0();
        regs->CMAR = memAddr;
        regs->CNDTR = len;
        regs->CCR.d32 = config.d32;
    }

    // Initiate a DMA transfer with preemption lockout where required
    void startTransferWithLock(volatile STM32_DMA_STREAM_REG_TYPE* regs, Config config, void* memAddr, size_t len)
    {
        onWithLock();
        regs->CMAR = memAddr;
        regs->CNDTR = len;
        regs->CCR.d32 = config.d32;
    }

    // Cancel a running DMA transfer with preemption lockout where required
    void cancelTransfer(volatile STM32_DMA_STREAM_REG_TYPE* stream)
    {
        stream->CCR.d32 = 0;
        offWithLock();
    }

    // Check if the specified DMA stream has a pending IRQ
    uint32_t checkIRQ(int controller, int stream)
    {
        return (STM32_DMA_REGS(controller).ISR.d32 >> (stream * 4)) & 0xf;
    }

    // Acknowledge IRQ and consider a transfer finished from a non-preemptable context
    void clearIRQFromPri0(int controller, int stream)
    {
#ifndef DMA_ALWAYS_ON
        bool on = STM32_DMA_STREAM_REGS(controller, stream).CCR.b.EN;
#endif
        STM32_DMA_STREAM_REGS(controller, stream).CCR.d32 = 0;
        STM32_DMA_REGS(controller).IFCR.d32 = 0xf << (stream * 4);
#ifndef DMA_ALWAYS_ON
        if (on) offFromPri0();
#endif
    }

    // Acknowledge IRQ and consider a transfer finished with preemption lockout where required
    void clearIRQWithLock(int controller, int stream)
    {
#ifndef DMA_ALWAYS_ON
        bool on = STM32_DMA_STREAM_REGS(controller, stream).CCR.b.EN;
#endif
        STM32_DMA_STREAM_REGS(controller, stream).CCR.d32 = 0;
        STM32_DMA_REGS(controller).IFCR.d32 = 0xf << (stream * 4);
#ifndef DMA_ALWAYS_ON
        if (on) offWithLock();
#endif
    }
}
