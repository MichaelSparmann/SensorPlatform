// A simple software pseudorandom number generator
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

// Algorithm taken from the C standard.

#include "global.h"
#include "random.h"
#include "sys/time.h"


namespace Random
{
    static uint32_t state;

    void init()
    {
        state = read_usec_timer();
    }

    void seed(uint32_t data)
    {
        state = data;
    }

    // Get 11 bits of randomness
    uint32_t random2K()
    {
        state *= 1103515245;
        state += 12345;
        return (state >> 16) & 0x7ff;
    }

    // Get 32 bits of randomness
    uint32_t random()
    {
        return (((random2K() << 11) ^ random2K()) << 10) ^ random2K();
    }
}
