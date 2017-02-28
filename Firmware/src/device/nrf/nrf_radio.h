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
#include "device/nrf/nrf_spi.h"


namespace NRF
{

    // NRF-style radio device abstraction: Generic registers shared with most clones
    class __attribute__((packed,aligned(4))) Radio : public SPI
    {
    public:
        enum Role
        {
            Role_PTX = 0,
            Role_PRX = 1,
        };

        enum CrcMode
        {
            CrcMode_None = 0,
            CrcMode_8Bit = 2,
            CrcMode_16Bit = 3,
        };

        enum Register
        {
            Reg_Config = 0x00,
            Reg_Status = 0x07,
            Reg_RxPipeAddress0 = 0x0a,
            Reg_RxPipeAddress1 = 0x0b,
            Reg_RxPipeAddress2 = 0x0c,
            Reg_RxPipeAddress3 = 0x0d,
            Reg_RxPipeAddress4 = 0x0e,
            Reg_RxPipeAddress5 = 0x0f,
            Reg_TxAddress = 0x10,
            Reg_RxDataLength0 = 0x11,
            Reg_RxDataLength1 = 0x12,
            Reg_RxDataLength2 = 0x13,
            Reg_RxDataLength3 = 0x14,
            Reg_RxDataLength4 = 0x15,
            Reg_RxDataLength5 = 0x16,
            Reg_FifoStatus = 0x17,
        };

        union __attribute__((packed)) Config
        {
            struct __attribute__((packed)) b
            {
                Role role : 1;
                bool powerUp : 1;
                CrcMode crcMode : 2;
                bool maskMaxRetrans : 1;
                bool maskDataSent : 1;
                bool maskDataReceived : 1;
                uint8_t : 1;
            } b;
            uint8_t d8;
            constexpr Config() : b{Role_PTX, false, CrcMode_None, false, false, false} {}
            constexpr Config(uint8_t d8) : d8{d8} {}
            constexpr Config(Role role, bool powerUp, CrcMode crcMode,
                             bool maskMaxRetrans, bool maskDataSent, bool maskDataReceived)
                : b{role, powerUp, crcMode, maskMaxRetrans, maskDataSent, maskDataReceived} {}
        };
    };

}
