#pragma once

// LCD driver (for PCD8544-based LCDs) for sensor node breadboard setup.
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


namespace LCD
{
    extern void init();
    extern void powerDown();
    extern void blank(bool blank, bool invert);
    extern void setVop(int vop);
    extern void setBias(int bias);
    extern void setTc(int tc);
    extern void timerTick();
    extern void clear();
    extern int putch(int row, int col, char c);
    extern int print(int row, int col, const char* text);
    extern int printf(int row, int col, const char* format, ...);
    extern int blit(int row, int col, const uint8_t* data, int len);
}
