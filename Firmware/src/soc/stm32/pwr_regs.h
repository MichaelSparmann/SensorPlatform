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


struct __attribute__((packed)) STM32_PWR_REG_TYPE
{
    union CR
    {
        uint32_t d32;
        struct __attribute__((packed))
        {
            uint32_t LPDS : 1;
            uint32_t PDDS : 1;
            uint32_t CWUF : 1;
            uint32_t CSBF : 1;
            uint32_t PVDE : 1;
            uint32_t PLS : 3;
            uint32_t DBP : 1;
            uint32_t FPDS : 1;
            uint32_t LPLVDS : 1;
            uint32_t MRUDS : 1;
            uint32_t : 1;
            uint32_t ADCDC1 : 1;
            uint32_t PMODE : 2;
            uint32_t ODEN : 1;
            uint32_t ODSWEN : 1;
            uint32_t UDEN : 2;
            uint32_t : 12;
        } b;
    } CR;
    union CSR
    {
        uint32_t d32;
        struct __attribute__((packed))
        {
#ifdef SOC_STM32F0
            uint32_t WUF : 1;
            uint32_t SBF : 1;
            uint32_t PVDO : 1;
            uint32_t VREFINTRDY : 1;
            uint32_t : 4;
            uint32_t EWUP1 : 1;
            uint32_t EWUP2 : 1;
            uint32_t EWUP3 : 1;
            uint32_t EWUP4 : 1;
            uint32_t EWUP5 : 1;
            uint32_t EWUP6 : 1;
            uint32_t EWUP7 : 1;
            uint32_t EWUP8 : 1;
            uint32_t : 16;
#else
            uint32_t WUF : 1;
            uint32_t SBF : 1;
            uint32_t PVDO : 1;
            uint32_t BRR : 1;
            uint32_t : 4;
            uint32_t EWUP : 1;
            uint32_t BRE : 1;
            uint32_t : 4;
            uint32_t REGRDY : 1;
            uint32_t : 1;
            uint32_t ODRDY : 1;
            uint32_t ODSWRDY : 1;
            uint32_t UDSWRDY : 2;
            uint32_t : 12;
#endif
        } b;
    } CSR;
};

#define STM32_PWR_REGS (*((volatile STM32_PWR_REG_TYPE*)0x40007000))

