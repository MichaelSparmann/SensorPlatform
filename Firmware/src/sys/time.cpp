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
#include "sys/time.h"

static uint32_t sys_time_usec_timer_high = 0;
static uint32_t sys_time_usec_timer_last_low = 0;

void TIME_OPTIMIZE udelay(unsigned int microseconds)
{
    unsigned int timeout = read_usec_timer() + microseconds + 1;
    while ((int)(read_usec_timer() - timeout) < 0);
}

// Implement 64bit microsecond timer on top of a 32 bit one, assuming
// that read_usec_timer64() is called at least once per 4294 seconds.
// Override this if a better platform-specific implementation is available.
__attribute__((weak)) TIME_OPTIMIZE int64_t read_usec_timer64()
{
    uint32_t low = (uint32_t)read_usec_timer();
    if (low < sys_time_usec_timer_last_low) sys_time_usec_timer_high++;
    sys_time_usec_timer_last_low = low;
    int64_t high = ((int64_t)sys_time_usec_timer_high) << 32;
    return high | low;
}
