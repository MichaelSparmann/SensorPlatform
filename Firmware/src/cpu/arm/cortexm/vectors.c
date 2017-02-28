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
#include "cpu/arm/cortexm/irq.h"
#include "cpu/arm/cortexm/init.h"
#include "cpu/arm/cortexm/cortexutil.h"
#include "sys/util.h"


__attribute__((weak)) void fault_handler()
{
    hang();
}

__attribute__((weak)) void unhandled_irq_handler()
{
    fault_handler();
}

#define CORTEXM_DEFINE_FAULT(name) __attribute__((weak,alias("fault_handler"))) void name ## _faulthandler();
#include "cpu/arm/cortexm/fault_defs.h"
#undef CORTEXM_DEFINE_FAULT
#define CORTEXM_DEFINE_IRQ(name) __attribute__((weak,alias("unhandled_irq_handler"))) void name ## _irqhandler();
#include CORTEXM_IRQ_DEF_FILE
#undef CORTEXM_DEFINE_IRQ

void _stack_end();  // Ugly hack: Not actually a function, but a symbol defined in the linker script

void (*const _irqvectors[])() __attribute__((used,section(".vectors"))) =
{
    _stack_end,
    init,
#define CORTEXM_DEFINE_FAULT(name) name ## _faulthandler,
#include "cpu/arm/cortexm/fault_defs.h"
#undef CORTEXM_DEFINE_FAULT
#define CORTEXM_DEFINE_IRQ(name) name ## _irqhandler,
#include CORTEXM_IRQ_DEF_FILE
#undef CORTEXM_DEFINE_IRQ
};

