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


namespace NRF
{

    // NRF-style SPI device abstraction: Low-level SPI bus access
    class __attribute__((packed,aligned(4))) SPI
    {
    public:
        enum Command
        {
            Cmd_ReadReg = 0x000,
            Cmd_WriteReg = 0x120,
            Cmd_Activate = 0x150,
            Cmd_GetRxSize = 0x060,
            Cmd_ReadPacket = 0x061,
            Cmd_WritePacket = 0x1a0,
            Cmd_WriteACKPayload = 0x1a8,
            Cmd_WriteBroadcastPacket = 0x1b0,
            Cmd_FlushTx = 0x0e1,
            Cmd_FlushRx = 0x0e2,
            Cmd_ReuseTx = 0x0e3,
            Cmd_GetStatus = 0x0ff,
        };

        union __attribute__((packed)) Status
        {
            struct __attribute__((packed)) b
            {
                bool txFull : 1;
                uint8_t rxPipe : 3;
                bool maxRetrans : 1;
                bool dataSent : 1;
                bool dataReceived : 1;
                uint8_t regBank : 1;
            } b;
            uint8_t d8;
            constexpr Status() : b{0, 0, 0, 0, 0, 0} {}
            constexpr Status(uint8_t byte) : d8{byte} {}
            constexpr Status(bool maxRetrans, bool dataSent, bool dataReceived)
                : b{0, 0, maxRetrans, dataSent, dataReceived, 0} {}
        };
    };

}
