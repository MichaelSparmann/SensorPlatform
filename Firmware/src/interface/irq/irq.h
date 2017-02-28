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


#ifdef __cplusplus
extern "C"
{
#endif
    void irq_enable(int irq, bool on);
    bool irq_get_pending(int irq);
    void irq_clear_pending(int irq);
    void irq_set_pending(int irq);
    void irq_set_priority(int irq, int priority);
    void irq_set_priority_grouping(int bits);
#ifdef __cplusplus
}
#endif
