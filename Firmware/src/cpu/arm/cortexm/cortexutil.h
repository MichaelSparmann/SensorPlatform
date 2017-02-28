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

#ifndef CORTEXUTIL_OPTIMIZE
#define CORTEXUTIL_OPTIMIZE __attribute__((optimize("-Os")))
#endif

// Cortex-M3/M4 bit banding helper macros
#if defined(CPU_ARM_CORTEX_M3) || defined(CPU_ARM_CORTEX_M4)
#define BB_SRAM(addr, bit) (*(uint32_t*)(0x22000000 + ((((uintptr_t)&(addr)) - 0x20000000) * 32) + (bit) * 4))
#define BB_PERIPH(addr, bit) (*(uint32_t*)(0x42000000 + ((((uintptr_t)&(addr)) - 0x40000000) * 32) + (bit) * 4))
#endif

// Push callee-saved registers, exchange stack pointer, and pop callee-saved registers.
// A very quick method for switching back and forth between two contexts.
// Call once for entry into the thread context and again to exit it.
#define CONTEXTSWITCH(thread) __asm__ volatile(" \
    mov R0, R8 \n\
    mov R1, R9 \n\
    mov R2, R10 \n\
    mov R3, R11 \n\
    push {R0-R7,LR} \n\
    mov R0, SP \n\
    ldr R1, =" thread " \n\
    ldr R2, [R1] \n\
    str R0, [R1] \n\
    mov SP, R2 \n\
    pop {R0-R3} \n\
    mov R8, R0 \n\
    mov R9, R1 \n\
    mov R10, R2 \n\
    mov R11, R3 \n\
    pop {R4-R7,PC} \n\
    .ltorg \n\
");

#ifdef __cplusplus
extern "C"
{
#endif

static inline void WFI()
{
    __asm__ volatile("wfi" ::: "memory");
}

static inline void WFE()
{
    __asm__ volatile("wfe" ::: "memory");
}

static inline void SEV()
{
    __asm__ volatile("sev");
}

static inline unsigned int read_ipsr()
{
    unsigned int result;
    __asm__ volatile("mrs %[result], ipsr" : [result] "=r" (result));
    return result;
}

void fault_handler();

#ifdef __cplusplus
}
#endif

