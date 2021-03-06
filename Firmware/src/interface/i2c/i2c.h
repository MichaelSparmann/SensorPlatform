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


namespace I2C
{

    enum Result
    {
        RESULT_OK = 0,
        RESULT_INVALID_STATE,
        RESULT_INVALID_ARGUMENT,
        RESULT_UNSUPPORTED,
        RESULT_UNKNOWN_ERROR,
        RESULT_NAK,
        RESULT_TIMEOUT,
        RESULT_COLLISION,
    };

    class __attribute__((packed,aligned(4))) Transaction final
    {
    public:
        uint32_t address : 10;
        uint32_t reserved : 6;
        uint32_t timeout : 8;
        uint32_t transferCount : 8;
        class __attribute__((packed,aligned(4))) Transfer final
        {
        public:
            enum __attribute__((packed)) Type
            {
                TYPE_TX,
                TYPE_RX,
                TYPE_CONT,
            } type;
            uint8_t reserved = 0;
            uint16_t len;
            union __attribute__((packed,aligned(4)))
            {
                const void* txbuf;
                void* rxbuf;
            };

            constexpr Transfer() : type(TYPE_TX), len(0), txbuf(NULL) {}
            constexpr Transfer(Type type, uint16_t len, const void* buf)
                : type(type), reserved(0), len(len), txbuf(buf) {}
        } transfers[];

        constexpr Transaction(uint32_t address, uint32_t transferCount)
            : address(address), reserved(0), timeout(0), transferCount(transferCount) {}
    };

    class __attribute__((packed,aligned(4))) Bus
    {
    public:
        virtual enum Result txn(const Transaction* txn) = 0;
        enum Result readRegs(int address, int reg, void* buf, int len);
        enum Result writeRegs(int address, int reg, const void* buf, int len);
    };

}
