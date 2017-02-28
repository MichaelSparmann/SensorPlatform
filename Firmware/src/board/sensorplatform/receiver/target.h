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


#define BOARD_SENSORPLATFORM_RECEIVER
#define SOC_STM32F2XXXB
#ifndef STM32_VOLTAGE
#define STM32_VOLTAGE 3300
#endif
#ifndef SENSORPLATFORM_CUSTOM_CLOCKING
#define STM32_CLOCKSOURCE_HSE
#define STM32_HSE_CRYSTAL
#define STM32_HSE_FREQUENCY 25000000
#define STM32_PLLIN_CLOCK 1000000
#endif
#include "soc/stm32/f2/target.h"
