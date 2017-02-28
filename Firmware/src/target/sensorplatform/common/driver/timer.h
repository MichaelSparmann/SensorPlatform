#pragma once

// SensorPlatform STM32 Hardware Timer Driver
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
#include "soc/stm32/timer_regs.h"


namespace Timer
{
    extern void start(volatile STM32_TIM_REG_TYPE* regs, int clkgate, int prescaler, int period);
    extern void stop(volatile STM32_TIM_REG_TYPE* regs, int clkgate);
    extern void updatePeriod(volatile STM32_TIM_REG_TYPE* regs, int period);
    extern void reset(volatile STM32_TIM_REG_TYPE* regs);
    extern uint32_t read(volatile STM32_TIM_REG_TYPE* regs);
    extern void enableIRQ(volatile STM32_TIM_REG_TYPE* regs, bool on);
    extern void scheduleIRQ(volatile STM32_TIM_REG_TYPE* regs, int time);
    extern void acknowledgeIRQ(volatile STM32_TIM_REG_TYPE* regs);
}
