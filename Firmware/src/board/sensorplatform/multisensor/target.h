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


#define BOARD_SENSORPLATFORM_MULTISENSOR
#define SOC_STM32F072
#define SOC_STM32F0XXX8
#ifndef STM32_VOLTAGE
#define STM32_VOLTAGE 3300
#endif
#ifndef SENSORPLATFORM_CUSTOM_CLOCKING
#define STM32_CLOCKSOURCE_HSI48
#define STM32_NO_PLL
#endif
#include "soc/stm32/f0/target.h"
