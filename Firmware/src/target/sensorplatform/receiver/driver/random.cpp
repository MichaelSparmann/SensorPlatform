// STM32F205 hardware random number generator driver
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
#include "random.h"
#include "soc/stm32/f2/rng_regs.h"
#include "interface/clockgate/clockgate.h"


namespace Random
{
    // Initialize hardware random number generator
    void init()
    {
        clockgate_enable(STM32_RNG_CLOCKGATE, true);
        union STM32_RNG_REG_TYPE::CR CR = { 0 };
        CR.b.RNGEN = true;
        STM32_RNG_REGS.CR.d32 = CR.d32;
    }

    // Get 32 bits of randomness
    uint32_t random()
    {
        while (!STM32_RNG_REGS.SR.b.DRDY);
        return STM32_RNG_REGS.DR;
    }
}
