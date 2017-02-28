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
#include "dma.h"
#include "sys/util.h"
#include "cpu/arm/cortexm/irq.h"
#include "interface/irq/irq.h"
#include "interface/clockgate/clockgate.h"
#include "soc/stm32/f2/rcc_regs.h"


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
        if (!users++)
        {
            // Enable DMA core clock
            *STM32_RCC_REGS.CLKGATES.d32 |= (1 << STM32_DMA1_CLOCKGATE) | (1 << STM32_DMA2_CLOCKGATE);
            // Keep DMA and memory clocks active during CPU sleep
            STM32_RCC_REGS.AHB1LPENR.d32 |= 0x00630000;  // DMA1, DMA2, SRAM1, SRAM2
        }
    }

    // Decrement usage counter and potentially shut off DMA from a non-preemptable context
    static void offFromPri0()
    {
        if (!--users)
        {
            // Disable DMA core clock
            *STM32_RCC_REGS.CLKGATES.d32 &= ~((1 << STM32_DMA1_CLOCKGATE) | (1 << STM32_DMA2_CLOCKGATE));
            // Stop DMA and memory clocks during CPU sleep
            STM32_RCC_REGS.AHB1LPENR.d32 &= ~0x00630000;  // DMA1, DMA2, SRAM1, SRAM2
        }
    }

    // Enable all clocks required for DMA with preemption lockout
    static void onWithLock()
    {
        __asm__ volatile("cpsid if");
        if (!users++)
        {
            *STM32_RCC_REGS.CLKGATES.d32 |= (1 << STM32_DMA1_CLOCKGATE) | (1 << STM32_DMA2_CLOCKGATE);
            STM32_RCC_REGS.AHB1LPENR.d32 |= 0x00630000;  // DMA1, DMA2, SRAM1, SRAM2
        }
        __asm__ volatile("cpsie if");
    }

    // Decrement usage counter and potentially shut off DMA with preemption lockout
    static void offWithLock()
    {
        __asm__ volatile("cpsid if");
        if (!--users)
        {
            *STM32_RCC_REGS.CLKGATES.d32 &= ~((1 << STM32_DMA1_CLOCKGATE) | (1 << STM32_DMA2_CLOCKGATE));
            STM32_RCC_REGS.AHB1LPENR.d32 &= ~0x00630000;  // DMA1, DMA2, SRAM1, SRAM2
        }
        __asm__ volatile("cpsie if");
    }
#endif

    // Initialize DMA hardware
    void init()
    {
        // If we don't want power management, we need to turn power on once on init
#ifdef DMA_ALWAYS_ON
        *STM32_RCC_REGS.CLKGATES.d32 |= (1 << STM32_DMA1_CLOCKGATE) | (1 << STM32_DMA2_CLOCKGATE);
        STM32_RCC_REGS.AHB1LPENR.d32 |= 0x00630000;  // DMA1, DMA2, SRAM1, SRAM2
#endif
    }

    // Configure peripheral-side address of a DMA stream
    void setPeripheralAddr(volatile STM32_DMA_STREAM_REG_TYPE* regs, volatile void* addr)
    {
        onWithLock();
        regs->PAR = (uint32_t)addr;
        offWithLock();
    }

    // Configure FIFO threshold of a DMA stream
    void setFIFOConfig(volatile STM32_DMA_STREAM_REG_TYPE* regs, bool direct, int threshold)
    {
        union STM32_DMA_STREAM_REG_TYPE::FCR FCR = { 0 };
        FCR.b.DMDIS = !direct;
        FCR.b.FTH = threshold;
        onWithLock();
        regs->FCR.d32 = FCR.d32;
        offWithLock();
    }

    // Initiate a DMA transfer from a non-preemptable context
    void startTransferFromPri0(volatile STM32_DMA_STREAM_REG_TYPE* regs, Config config, void* memAddr, size_t len)
    {
        onFromPri0();
        regs->M0AR = (uint32_t)memAddr;
        regs->NDTR = len;
        regs->CR.d32 = config.d32;
    }

    // Initiate a DMA transfer with preemption lockout where required
    void startTransferWithLock(volatile STM32_DMA_STREAM_REG_TYPE* regs, Config config, void* memAddr, size_t len)
    {
        onWithLock();
        regs->M0AR = (uint32_t)memAddr;
        regs->NDTR = len;
        regs->CR.d32 = config.d32;
    }

    // Cancel a running DMA transfer from a non-preemptable context
    void cancelTransferFromPri0(volatile STM32_DMA_STREAM_REG_TYPE* regs, int controller, int stream)
    {
        regs->CR.d32 = 0;
        clearIRQFromPri0(controller, stream);
    }

    // Cancel a running DMA transfer with preemption lockout where required
    void cancelTransferWithLock(volatile STM32_DMA_STREAM_REG_TYPE* regs, int controller, int stream)
    {
        regs->CR.d32 = 0;
        clearIRQWithLock(controller, stream);
    }

    // Acknowledge IRQ and consider a transfer finished from a non-preemptable context
    void clearIRQFromPri0(int controller, int stream)
    {
        STM32_DMA_REGS(controller).IFCR.d32[stream >> 2] = 0x3d << ((stream & 0x1) * 6 + (stream & 0x2) * 8);
        offFromPri0();
    }

    // Acknowledge IRQ and consider a transfer finished with preemption lockout where required
    void clearIRQWithLock(int controller, int stream)
    {
        STM32_DMA_REGS(controller).IFCR.d32[stream >> 2] = 0x3d << ((stream & 0x1) * 6 + (stream & 0x2) * 8);
        offWithLock();
    }
}
