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
#include "interface/gpio/gpio.h"


namespace STM32
{
    class __attribute__((packed,aligned(4))) EXTI final
    {
    public:
        enum Source
        {
            SOURCE_GPIO0 = 0,
            SOURCE_GPIO1 = 1,
            SOURCE_GPIO2 = 2,
            SOURCE_GPIO3 = 3,
            SOURCE_GPIO4 = 4,
            SOURCE_GPIO5 = 5,
            SOURCE_GPIO6 = 6,
            SOURCE_GPIO7 = 7,
            SOURCE_GPIO8 = 8,
            SOURCE_GPIO9 = 9,
            SOURCE_GPIO10 = 10,
            SOURCE_GPIO11 = 11,
            SOURCE_GPIO12 = 12,
            SOURCE_GPIO13 = 13,
            SOURCE_GPIO14 = 14,
            SOURCE_GPIO15 = 15,
#if defined(SOC_STM32F0)
            SOURCE_PVD = 16,
            SOURCE_RTC_ALARM = 17,
            SOURCE_USB = 18,
            SOURCE_RTC_TAMP_TS = 19,
            SOURCE_RTC_WAKEUP = 20,
            SOURCE_COMP1 = 21,
            SOURCE_COMP2 = 22,
            SOURCE_I2C1 = 23,
            SOURCE_USART1 = 25,
            SOURCE_USART2 = 26,
            SOURCE_CEC = 27,
            SOURCE_USART3 = 28,
            SOURCE_VDDIO2 = 31,
#elif defined(SOC_STM32F2)
            SOURCE_PVD = 16,
            SOURCE_RTC_ALARM = 17,
            SOURCE_OTG_FS = 18,
            SOURCE_ETH = 19,
            SOURCE_OTG_HS = 20,
            SOURCE_RTC_TAMP_TS = 21,
            SOURCE_RTC_WAKEUP = 22,
#endif
        };

        enum Edge
        {
            EDGE_NONE = 0,
            EDGE_RISING = 1,
            EDGE_FALLING = 2,
            EDGE_BOTH = EDGE_RISING | EDGE_FALLING,
        };

        struct __attribute__((packed)) Config
        {
            bool irq : 1;
            bool evt : 1;
            Edge edge : 2;
            constexpr Config(bool irq, bool evt, Edge edge) : irq(irq), evt(evt), edge(edge) {}
        };

        static void configure(Source source, Config config);
        static void configure(::GPIO::Pin pin, Config config);
        static void enableIRQ(EXTI::Source source, bool on);
        static void enableIRQ(::GPIO::Pin pin, bool on);
        static void enableEVT(EXTI::Source source, bool on);
        static void enableEVT(::GPIO::Pin pin, bool on);
        static bool getPending(Source source);
        static bool getPending(::GPIO::Pin pin);
        static void clearPending(Source source);
        static void clearPending(::GPIO::Pin pin);
    };
}
