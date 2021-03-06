#pragma once

// Clock gate management driver for STM32F205
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


// TODO: This could probably make use of bit banding to get rid of the WithLock variants.

#include "global.h"
#include "soc/stm32/f2/rcc_regs.h"


namespace Clock
{
    // Turn on a clock to a peripheral from a non-preemptable context
    inline void __attribute__((always_inline)) onFromPri0(int clkgate)
    {
        STM32_RCC_REGS.CLKGATES.d32[clkgate >> 5] |= 1 << (clkgate & 0x1f);
    }

    // Turn off a clock to a peripheral from a non-preemptable context
    inline void __attribute__((always_inline)) offFromPri0(int clkgate)
    {
        STM32_RCC_REGS.CLKGATES.d32[clkgate >> 5] &= ~(1 << (clkgate & 0x1f));
    }

    // Turn on a clock to a peripheral with preemption lockout
    inline void __attribute__((always_inline)) onWithLock(int clkgate)
    {
        volatile uint32_t* reg = &STM32_RCC_REGS.CLKGATES.d32[clkgate >> 5];
        int bit = clkgate & 0x1f;
        __asm__ volatile("cpsid if");
        *reg |= 1 << bit;
        __asm__ volatile("cpsie if");
    }

    // Turn off a clock to a peripheral with preemption lockout
    inline void __attribute__((always_inline)) offWithLock(int clkgate)
    {
        volatile uint32_t* reg = &STM32_RCC_REGS.CLKGATES.d32[clkgate >> 5];
        int bit = clkgate & 0x1f;
        __asm__ volatile("cpsid if");
        *reg &= ~(1 << bit);
        __asm__ volatile("cpsie if");
    }

}
