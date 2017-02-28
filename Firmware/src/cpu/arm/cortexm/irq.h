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
#include "cpu/arm/cortexm/cortexutil.h"
#include "interface/irq/irq.h"

typedef enum irq_number
{
    __dummy_init_value__ = -15,
#define CORTEXM_DEFINE_FAULT(name) name ## _IRQn,
#include "cpu/arm/cortexm/fault_defs.h"
#undef CORTEXM_DEFINE_FAULT
#define CORTEXM_DEFINE_IRQ(name) name ## _IRQn,
#include CORTEXM_IRQ_DEF_FILE
#undef CORTEXM_DEFINE_IRQ
} IRQn_Type;

#ifdef __cplusplus
extern "C"
{
#endif
    extern void (*const _irqvectors[])() __attribute__((section(".vectors")));

    void unhandled_irq_handler();
#ifdef __cplusplus
}
#endif

