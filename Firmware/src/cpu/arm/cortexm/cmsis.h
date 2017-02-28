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
#include "cpu/arm/cortexm/irq.h"

#ifdef CPU_ARM_CORTEX_M0
#include "lib/cmsis/core_cm0.h"
#endif
#ifdef CPU_ARM_CORTEX_M0p
#include "lib/cmsis/core_cm0plus.h"
#endif
#ifdef CPU_ARM_CORTEX_M3
#include "lib/cmsis/core_cm3.h"
#endif
#ifdef CPU_ARM_CORTEX_M4
#include "lib/cmsis/core_cm4.h"
#endif

