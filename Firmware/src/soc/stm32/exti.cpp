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
#include "soc/stm32/exti.h"
#include "soc/stm32/exti_regs.h"
#include "soc/stm32/syscfg_regs.h"
#include "soc/stm32/gpio.h"
#include "interface/clockgate/clockgate.h"
#include "sys/util.h"


namespace STM32
{
    void EXTI::configure(EXTI::Source source, Config config)
    {
        STM32_EXTI_REGS.IMR = (STM32_EXTI_REGS.IMR & ~(1 << source)) | (config.irq << source);
        STM32_EXTI_REGS.EMR = (STM32_EXTI_REGS.EMR & ~(1 << source)) | (config.evt << source);
        STM32_EXTI_REGS.RTSR = (STM32_EXTI_REGS.RTSR & ~(1 << source)) | ((!!(config.edge & EDGE_RISING)) << source);
        STM32_EXTI_REGS.FTSR = (STM32_EXTI_REGS.FTSR & ~(1 << source)) | ((!!(config.edge & EDGE_FALLING)) << source);
    }

    void EXTI::configure(::GPIO::Pin pin, Config config)
    {
        int pinnum = STM32::GPIO::getPin(pin);
        int shift = (pinnum & 3) << 2;
        bool old = clockgate_enable_getold(STM32_SYSCFG_CLOCKGATE, true);
        STM32_SYSCFG_REGS.EXTICR[pinnum >> 2] = (STM32_SYSCFG_REGS.EXTICR[pinnum >> 2] & ~(0xf << shift))
                                              | (STM32::GPIO::getPort(pin) << shift);
        clockgate_enable(STM32_SYSCFG_CLOCKGATE, old);
        configure((Source)pinnum, config);
    }

    void EXTI::enableIRQ(EXTI::Source source, bool on)
    {
        STM32_EXTI_REGS.IMR = (STM32_EXTI_REGS.IMR & ~(1 << source)) | (on << source);
    }

    void EXTI::enableIRQ(::GPIO::Pin pin, bool on)
    {
        enableIRQ((Source)STM32::GPIO::getPin(pin), on);
    }

    void EXTI::enableEVT(EXTI::Source source, bool on)
    {
        STM32_EXTI_REGS.EMR = (STM32_EXTI_REGS.EMR & ~(1 << source)) | (on << source);
    }

    void EXTI::enableEVT(::GPIO::Pin pin, bool on)
    {
        enableEVT((Source)STM32::GPIO::getPin(pin), on);
    }

    bool EXTI::getPending(EXTI::Source source)
    {
        return (STM32_EXTI_REGS.PR >> source) & 1;
    }

    bool EXTI::getPending(::GPIO::Pin pin)
    {
        return getPending((Source)STM32::GPIO::getPin(pin));
    }

    void EXTI::clearPending(EXTI::Source source)
    {
        STM32_EXTI_REGS.PR = 1 << source;
    }

    void EXTI::clearPending(::GPIO::Pin pin)
    {
        clearPending((Source)STM32::GPIO::getPin(pin));
    }
}
