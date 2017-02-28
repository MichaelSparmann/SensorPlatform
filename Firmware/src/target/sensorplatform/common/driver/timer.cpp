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
#include "timer.h"
#include "soc/stm32/timer_regs.h"
#include "interface/clockgate/clockgate.h"
#include "sys/time.h"

#ifdef SOC_STM32F0
#include "soc/stm32/f0/rcc.h"
#else
#include "soc/stm32/f2/rcc.h"
#endif


namespace Timer
{
    // Start a hardware timer with the given prescaler and period divider.
    // The timer will periodically trigger update interrupts,
    // the first of which will fire immediately!
    void start(volatile STM32_TIM_REG_TYPE* regs, int clkgate, int prescaler, int period)
    {
        // Set up timer clocking
        clockgate_enable(clkgate, true);
        regs->PSC = prescaler - 1;
        regs->ARR = period - 1;
        
        // Trigger update event (apply the PSC/ARR values and reset the timer's counters)
        union STM32_TIM_REG_TYPE::EGR EGR = { 0 };
        EGR.b.UG = true;
        regs->EGR.d32 = EGR.d32;
        
        // Enable update IRQ (will be pending because of the update trigger above)
        union STM32_TIM_REG_TYPE::DIER DIER = { 0 };
        DIER.b.UIE = true;
        regs->DIER.d32 = DIER.d32;
        
        // Start the timer clock, enable double-buffering for ARR
        union STM32_TIM_REG_TYPE::CR1 CR1 = { 0 };
        CR1.b.ARPE = true;
        CR1.b.CEN = true;
        regs->CR1.d32 = CR1.d32;
    }

    // Stop a hardware timer, preventing it from generating any more IRQs.
    void stop(volatile STM32_TIM_REG_TYPE* regs, int clkgate)
    {
        // Stop the timer
        regs->CR1.d32 = 0;
        // Clear pending IRQs
        acknowledgeIRQ(regs);
        // Stop clocks (save power)
        clockgate_enable(clkgate, false);
    }

    // Update the period of a hardware timer
    void updatePeriod(volatile STM32_TIM_REG_TYPE* regs, int period)
    {
        regs->ARR = period - 1;
    }

    // Reset the counters of a hardware timer (applies PSC/ARR and clears any pending IRQs)
    void reset(volatile STM32_TIM_REG_TYPE* regs)
    {
        // Trigger update event (apply the PSC/ARR values and reset the timer's counters)
        union STM32_TIM_REG_TYPE::EGR EGR = { 0 };
        EGR.b.UG = true;
        regs->EGR.d32 = EGR.d32;
        
        // Clear all pending IRQs (especially the one triggered by the update event above)
        acknowledgeIRQ(regs);
    }

    // Read a hardware timer's current counter value
    uint32_t read(volatile STM32_TIM_REG_TYPE* regs)
    {
        return regs->CNT;
    }

    // Enable/disable capture/compare IRQ on a hardware timer
    void enableIRQ(volatile STM32_TIM_REG_TYPE* regs, bool on)
    {
        regs->DIER.b.CC1IE = on;
    }

    // Set the compare value of a hardware timer (used for e.g. non-periodic wakeups)
    void scheduleIRQ(volatile STM32_TIM_REG_TYPE* regs, int time)
    {
        regs->CCR1 = time;
    }

    // Clear all pending IRQs of a hardware timer
    void acknowledgeIRQ(volatile STM32_TIM_REG_TYPE* regs)
    {
        regs->SR.d32 = 0;
    }
}


// This replaces the ARM core's SysTick timer with a STM32 32-bit microsecond timer (TICK_TIMER),
// which can be read more efficiently and which doesn't need special wrap-around handling.
void TIME_OPTIMIZE time_init()
{
    // Set up clocking
    clockgate_enable(TICK_TIMER_CLK, true);
    TICK_TIMER.PSC = (TICK_TIMER_FREQ / 1000000) - 1;
    TICK_TIMER.ARR = 0xffffffff;
    
    // Trigger update event (applies PSC/ARR)
    union STM32_TIM_REG_TYPE::EGR EGR = { 0 };
    EGR.b.UG = true;
    TICK_TIMER.EGR.d32 = EGR.d32;
    
    // Start the timer's clock
    union STM32_TIM_REG_TYPE::CR1 CR1 = { 0 };
    CR1.b.CEN = true;
    TICK_TIMER.CR1.d32 = CR1.d32;
}

// Read the current 32-bit microsecond timer value
unsigned int TIME_OPTIMIZE read_usec_timer()
{
    return TICK_TIMER.CNT;
}
