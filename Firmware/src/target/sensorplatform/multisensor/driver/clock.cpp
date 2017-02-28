// STM32F072 Real Time Clock driver and wakeup management
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
#include "clock.h"
#include "soc/stm32/exti.h"
#include "soc/stm32/pwr_regs.h"
#include "soc/stm32/f0/rtc_regs.h"
#include "soc/stm32/f0/crs_regs.h"
#include "sys/util.h"
#include "../irq.h"


namespace Clock
{
    // Initialize RTC
    void init()
    {
        // Start up LSE oscillator (32768Hz quartz), wait for it to start up.
        union STM32_RCC_REG_TYPE::CSR CSR = { 0 };
        CSR.b.LSION = true;
        STM32_RCC_REGS.CSR.d32 = CSR.d32;
        while (!STM32_RCC_REGS.CSR.b.LSIRDY);

        // Reset RTC
        union STM32_RCC_REG_TYPE::BDCR BDCR = { STM32_RCC_REGS.BDCR.d32 };
        BDCR.b.BDRST = true;
        STM32_RCC_REGS.BDCR.d32 = BDCR.d32;
        BDCR.b.BDRST = false;
        BDCR.b.RTCEN = true;
        BDCR.b.RTCSEL = BDCR.b.RTCSEL_LSI;
        STM32_RCC_REGS.BDCR.d32 = BDCR.d32;
        STM32_RTC_REGS.WPR = 0xca;
        STM32_RTC_REGS.WPR = 0x53;
        union STM32_RTC_REG_TYPE::ISR ISR = { 0 };
        ISR.b.INIT = true;
        STM32_RTC_REGS.ISR.d32 = ISR.d32;
        while (!STM32_RTC_REGS.ISR.b.INITF);

        // Configure RTC prescaler
        union STM32_RTC_REG_TYPE::PRER PRER = { 0 };
        PRER.b.PREDIV_A = (1 << 1) - 1;
        PRER.b.PREDIV_S = (40000 >> 1) - 1;
        STM32_RTC_REGS.PRER.d32 = PRER.d32;

        // Set date and time to zero
        STM32_RTC_REGS.TR.d32 = 0;
        STM32_RTC_REGS.DR.d32 = 0;

        // Set up IRQ
        STM32_RTC_REGS.ISR.d32 = 0;
        STM32::EXTI::configure(STM32::EXTI::SOURCE_RTC_WAKEUP,
                               STM32::EXTI::Config(true, false, STM32::EXTI::EDGE_RISING));
    }

    // Disable the RTC periodic wakeup timer
    void disableWakeup()
    {
        STM32_RTC_REGS.CR.d32 = 0;  // clear WUTE
    }

    // Program RTC periodic wakeup interval (in ~0.4ms units)
    void setWakeupInterval(uint16_t interval)
    {
        // Wait for the wakeup timer registers to be writable
        if (!STM32_RTC_REGS.ISR.b.WUTWF)
        {
            disableWakeup();
            while (!STM32_RTC_REGS.ISR.b.WUTWF);
        }
        // Program interval
        STM32_RTC_REGS.WUTR = interval;
        // Enable the wakeup timer and let it generate IRQs
        union STM32_RTC_REG_TYPE::CR CR = { 0 };
        CR.b.WUCKSEL = 0;  // RTCCLK (LSI, ~40kHz) / 16
        CR.b.WUTE = true;
        CR.b.WUTIE = true;
        STM32_RTC_REGS.CR.d32 = CR.d32;
    }

    // Trim local HSI48 oscillator based on captured remote timestamps.
    // Will return true if the oscillator is with an accuracy window of
    // maxDeviation across the last remoteDelta.
    bool trim(int remoteDelta, int localDelta, int maxDeviation)
    {
        // Trim if the oscillator is out by more than 833ppm
        int error = (localDelta - remoteDelta) * 1200 / remoteDelta;
        // If the error is huge, assume a bad measurement and don't trim.
        if (error > 60 || error < -60) return false;
        if (error)
        {
            // Trim HSI48
            onFromPri0(STM32_CRS_CLOCKGATE);
            if (error > 0) STM32_CRS_REGS.CR.b.TRIM--;
            else STM32_CRS_REGS.CR.b.TRIM++;
            offFromPri0(STM32_CRS_CLOCKGATE);
            return false;
        }
        // Check if the error is within bounds
        return ABS(localDelta - remoteDelta) < maxDeviation;
    }

}
