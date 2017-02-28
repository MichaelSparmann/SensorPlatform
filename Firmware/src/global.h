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


#ifndef ASM_FILE
#ifdef __clang__
#define TYPE_NORETURN void __attribute__((noreturn))
typedef unsigned __INT8_TYPE__ uint8_t;
typedef unsigned __INT16_TYPE__ uint16_t;
typedef unsigned __INT32_TYPE__ uint32_t;
typedef unsigned __INT64_TYPE__ uint64_t;
typedef unsigned __INTPTR_TYPE__ uintptr_t;
#else
#define TYPE_NORETURN void
typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __UINTPTR_TYPE__ uintptr_t;
#endif
typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;
typedef __SIZE_TYPE__ size_t;
typedef __INTPTR_TYPE__ intptr_t;
#ifndef __cplusplus
typedef uint8_t bool;
#define true 1
#define false 0
#endif
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#include "config.h"

