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


#ifndef TIME_OPTIMIZE
#define TIME_OPTIMIZE
#endif


#define TIME_AFTER(a, b) ((long)(b) - (long)(a) < 0)
#define TIME_BEFORE(a, b) TIME_AFTER(b, a)
#define TIMEOUT_SETUP(a) (read_usec_timer() + (a))
#define TIMEOUT_EXPIRED(a) TIME_AFTER(read_usec_timer(), a)

#ifdef __cplusplus
extern "C"
{
#endif
extern void time_init();
extern unsigned int read_usec_timer();
extern int64_t read_usec_timer64();
extern void udelay(unsigned int microseconds);
#ifdef __cplusplus
}
#endif
