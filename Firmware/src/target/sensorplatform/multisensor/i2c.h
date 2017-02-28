#pragma once

// Task-aware STM32 I2C driver for sensor node, implements generic I2C bus interface
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
#include "interface/i2c/i2c.h"
#include "soc/stm32/f0/i2c.h"


class __attribute__((packed,aligned(4))) I2CBus final : public I2C::Bus
{
    const uint8_t index;  // Which I2C interface number is controlled by this instance
    bool initialized = false;  // Whether the I2C interface has already been configured
    const ::I2C::Transaction* volatile curTxn = NULL;  // Transaction which is in progress
    volatile bool busy = false;  // Whether a transaction is not completed yet
    volatile ::I2C::Result error = ::I2C::RESULT_OK;  // Result of the ongoing transaction
    uint8_t xfer = 0;  // Which transfer within the transaction is currently in progress
    uint8_t* buf = NULL;  // Transfer buffer pointer (incremented after every byte)
    uint32_t totallen = 0;  // How many bytes are left before the next START/STOP
    uint16_t len = 0;  // How many bytes are left in the current transfer
    bool tx = false;  // Whether the current transfer is master to slave
    bool last = false;  // Whether the current transfer is the last one of the transaction

    constexpr I2CBus(int index) : index(index) {}
    void start();
    void advance();
    void advanceIfNecessary();

public:
    void init();
    void setTiming(STM32::I2C::Timing timing);
    void setTimeout(STM32::I2C::Timeout timing);
    enum ::I2C::Result txn(const I2C::Transaction* txn);
    void irqHandler();

    static I2CBus I2C1;
    static I2CBus I2C2;
};
