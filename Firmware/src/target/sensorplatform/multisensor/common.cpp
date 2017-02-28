// Sensor node global variables and functions
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
#include "common.h"
#include "soc/stm32/gpio.h"
#include "sys/time.h"


// Global node configuration data
Config config;

// Main measurement data and sensor configuration buffer
MainBuf mainBuf;

// Main buffer block sequence numbers within measurement
uint32_t mainBufSeq[];

// Main buffer valid data flag. False before the block has been filled or while it's being written.
bool mainBufValid[];


// Blink out a binary error code and (if not a debug build) reboot after doing so 8 times.
void error(ErrorCode code)
{
    // Lock out IRQs to prevent any other code from running (and making the mess even worse)
    enter_critical_section();
    
    // If a debugger is attached, trigger a breakpoint.
    // If not, this is ignored by the CPU hard fault handler.
    breakpoint();
    
#ifdef DEBUG
    while (true)
#else
    for (int i = 0; i < 8; i++)
#endif
    {
        // Blink out error code with a manchester encoding:
        // 500ms flash, 500ms gap (start of error code)
        GPIO::setLevel(PIN_LED, true);
        udelay(500000);
        GPIO::setLevel(PIN_LED, false);
        udelay(500000);
        for (uint32_t d = code; d; d >>= 1)
        {
            // Blink out the code with the LSB first:
            // 500ms per bit, 0 is 50ms on time, 1 is 200ms on time.
            GPIO::setLevel(PIN_LED, true);
            udelay((d & 1) ? 200000 : 50000);
            GPIO::setLevel(PIN_LED, false);
            udelay((d & 1) ? 300000 : 450000);
        }
        // 3 seconds off time before the next repetition of the error code.
        udelay(3000000);
    }
    
    // If this isn't a debug build, reboot after blinking the error code 8 times.
    reset();
}
