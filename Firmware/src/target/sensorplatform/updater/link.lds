// SensorPlatform Updater Linker Script
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

ENTRY(_startup)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x1000
}

SECTIONS
{

    .stack (NOLOAD) :
    {
        _stack = .;
        . += 0x100;
        _stack_end = .;
    }

    .text :
    {
        _text = .;
        KEEP(*(.startup))
        *(.text)
        *(.text.*)
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        _text_end = .;
        _data = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _data_end = .;
    } >SRAM

    .bss (NOLOAD) :
    {
        _bss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        *(.dmabss)
        *(.dmabss.*)
        . = ALIGN(4);
        _bss_end = .;
    } >SRAM


    DISCARD :
    {
        *(.note.*)
    }

    .stab            0 : { *(.stab) }
    .stabstr         0 : { *(.stabstr) }
    .stab.excl       0 : { *(.stab.excl) }
    .stab.exclstr    0 : { *(.stab.exclstr) }
    .stab.index      0 : { *(.stab.index) }
    .stab.indexstr   0 : { *(.stab.indexstr) }
    .comment         0 : { *(.comment) }
    .debug           0 : { *(.debug) }
    .line            0 : { *(.line) }
    .debug_srcinfo   0 : { *(.debug_srcinfo) }
    .debug_sfnames   0 : { *(.debug_sfnames) }
    .debug_aranges   0 : { *(.debug_aranges) }
    .debug_pubnames  0 : { *(.debug_pubnames) }
    .debug_info      0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev    0 : { *(.debug_abbrev) }
    .debug_line      0 : { *(.debug_line) }
    .debug_frame     0 : { *(.debug_frame) }
    .debug_str       0 : { *(.debug_str) }
    .debug_loc       0 : { *(.debug_loc) }
    .debug_macinfo   0 : { *(.debug_macinfo) }
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }

}
