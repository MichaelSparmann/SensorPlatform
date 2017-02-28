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
#include "interface/i2c/i2c.h"
#include "sys/util.h"


enum I2C::Result I2C::Bus::readRegs(int address, int reg, void* buf, int len)
{
    if (address < 0 || address > 1023) return RESULT_INVALID_ARGUMENT;
    if (reg < 0 || reg > 255) return RESULT_INVALID_ARGUMENT;
    struct __attribute__((packed,aligned(4)))
    {
        Transaction info;
        Transaction::Transfer transfers[2];
    } txn =
    {
        Transaction(address, ARRAYLEN(txn.transfers)),
        {
            Transaction::Transfer(Transaction::Transfer::TYPE_TX, 1, &reg),
            Transaction::Transfer(Transaction::Transfer::TYPE_RX, len, buf),
        }
    };
    return this->txn(&txn.info);
}

enum I2C::Result I2C::Bus::writeRegs(int address, int reg, const void* buf, int len)
{
    if (address < 0 || address > 1023) return RESULT_INVALID_ARGUMENT;
    if (reg < 0 || reg > 255) return RESULT_INVALID_ARGUMENT;
    struct __attribute__((packed,aligned(4)))
    {
        Transaction info;
        Transaction::Transfer transfers[2];
    } txn =
    {
        Transaction(address, ARRAYLEN(txn.transfers)),
        {
            Transaction::Transfer(Transaction::Transfer::TYPE_TX, 1, &reg),
            Transaction::Transfer(Transaction::Transfer::TYPE_CONT, len, buf),
        }
    };
    return this->txn(&txn.info);
}
