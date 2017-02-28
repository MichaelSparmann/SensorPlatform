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
#include "sys/init.h"
#include "app/main.h"

// C++ compiler/linker magic to call constructors of global objects during initialization
extern void (*_init_array[])(), (*_init_array_end[])();

void sys_init()
{
    // Call constructors of global objects
    for (void (**i)() = _init_array; i < _init_array_end; i++) (*i)();

    // Run main application program
    main();
}
