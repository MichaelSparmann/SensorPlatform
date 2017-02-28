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
#include "i2c.h"
#include "soc/stm32/f0/i2c_regs.h"
#include "soc/stm32/f0/rcc_regs.h"
#include "interface/clockgate/clockgate.h"
#include "interface/resetline/resetline.h"
#include "sys/util.h"
#include "sys/time.h"
#include "driver/clock.h"
#include "irq.h"
#include "sensortask.h"


// System bus reset line numbers for the I2C interfaces (for error recovery)
uint8_t const i2c_resetline[] =
{
    STM32_I2C1_RESETLINE,
    STM32_I2C2_RESETLINE,
};

// One-time bus initialization
void I2CBus::init()
{
    // Switch I2C clock source to system clock (48MHz)
    STM32_RCC_REGS.CFGR3.b.I2C1SW = STM32_RCC_REGS.CFGR3.b.I2C1SW_SYSCLK;
    // Configure signal timing and timeouts
    setTiming(STM32::I2C::Timing(5, 9, 3, 3, 3));  // 400kHz
    setTimeout(STM32::I2C::Timeout(true, 233, false, 0));  // max. 10ms SCL low time
}

// Adjust signal timing (bus frequency and setup/hold times)
void I2CBus::setTiming(STM32::I2C::Timing timing)
{
    volatile STM32_I2C_REG_TYPE* regs = &STM32_I2C_REGS(index);
    Clock::onWithLock(STM32_I2C_CLOCKGATE(index));
    regs->TIMINGR.d32 = timing.d32;
    Clock::offWithLock(STM32_I2C_CLOCKGATE(index));
}

// Adjust clock stretching timeout
void I2CBus::setTimeout(STM32::I2C::Timeout timeout)
{
    volatile STM32_I2C_REG_TYPE* regs = &STM32_I2C_REGS(index);
    Clock::onWithLock(STM32_I2C_CLOCKGATE(index));
    regs->TIMEOUTR.d32 = timeout.d32;
    Clock::offWithLock(STM32_I2C_CLOCKGATE(index));
}

// Process an I2C bus transaction
enum I2C::Result I2CBus::txn(const I2C::Transaction* txn)
{
    volatile STM32_I2C_REG_TYPE* regs = &STM32_I2C_REGS(index);
    
    // Initialize state
    error = ::I2C::RESULT_OK;
    curTxn = txn;
    xfer = 0xff;
    busy = true;
    
    // Enable I2C interface clock gate
    Clock::onWithLock(STM32_I2C_CLOCKGATE(index));
    
    // Configure I2C interface in master mode
    union STM32_I2C_REG_TYPE::CR1 CR1 = { 0 };
    CR1.b.TCIE = true;  // Transfer completion interrupt enable
    CR1.b.TXIE = true;  // Byte transmitted interrupt enable
    CR1.b.RXIE = true;  // Byte received interrupt enable
    CR1.b.STOPIE = true;  // STOP condition sent interrupt enable
    CR1.b.ERRIE = true;  // Error interrupt enable
    CR1.b.PE = true;  // Peripheral enable
    regs->CR1.d32 = CR1.d32;
    
    // Start sending the first START condition
    start();
    
    // A timeout (in 100us steps) can be specified in the transaction.
    // If that is set to zero, a generous timeout of 10 seconds is applied as a fail-safe.
    int timeout = TIMEOUT_SETUP(100 * (txn->timeout ? txn->timeout : 100000));
    
    // Wait until the transaction completes or the timeout expires
    while (busy && !TIMEOUT_EXPIRED(timeout)) SensorTask::yield();
    
    if (busy)
    {
        // The timeout has expired and the transaction wasn't completed.
        // If we noticed any error so far, report that. Otherwise report a timeout error.
        if (error == ::I2C::RESULT_OK) error = ::I2C::RESULT_TIMEOUT;
        // Reset the I2C peripheral in case the hardware has locked up.
        resetline_assert(i2c_resetline[index], true);
        udelay(1);
        resetline_assert(i2c_resetline[index], false);
    }
    
    // Turn off I2C interface clock gate and return the result of the transaction
    Clock::offWithLock(STM32_I2C_CLOCKGATE(index));
    return error;
}

// Figure out how to handle the current transfer after moving to a new one
void I2CBus::advance()
{
    while (xfer < curTxn->transferCount)
    {
        // We are at the beginning of a new transfer.
        // Check if that transfer switches direction and keep track of that if so.
        // If it is a continuation, just update the buffer pointer and remaining transfer length.
        if (curTxn->transfers[xfer].type == ::I2C::Transaction::Transfer::TYPE_TX) tx = true;
        else if (curTxn->transfers[xfer].type == ::I2C::Transaction::Transfer::TYPE_RX) tx = false;
        else if (!curTxn->transfers[xfer].len)
        {
            // This is a zero-length continuation transfer.
            // We can just ignore that and skip to the next one.
            xfer++;
            continue;
        }
        buf = (uint8_t*)curTxn->transfers[xfer].rxbuf;
        len = curTxn->transfers[xfer].len;
        break;
    }
    
    // If we didn't find anything to do above, set transfer length to zero.
    if (xfer >= curTxn->transferCount) len = 0;
    
    // Figure out how many bytes are to be transferred before the next START/STOP condition.
    // That includes all immediately following continuation transfers.
    totallen = len;
    int i;
    for (i = xfer + 1; i < curTxn->transferCount; i++)
        if (curTxn->transfers[i].type == ::I2C::Transaction::Transfer::TYPE_CONT)
            totallen += curTxn->transfers[i].len;
        else break;
        
    // Check if this is the last transfer of the transaction
    last = i == curTxn->transferCount;
}

// Move on the the next transfer if we are at the end of the current one
void I2CBus::advanceIfNecessary()
{
    // Move until we are either at the next START/STOP or have data to transfer
    while (!len && totallen)
    {
        xfer++;
        advance();
    }
}

// Signal a START condition on the bus and move on to the next transfer
void I2CBus::start()
{
    volatile STM32_I2C_REG_TYPE* regs = &STM32_I2C_REGS(index);
    
    // Move to the next transfer and figure out how to handle it.
    xfer++;
    advance();
    
    // Configure I2C interface accordingly and signal a START condition
    union STM32_I2C_REG_TYPE::CR2 CR2 = { 0 };
    CR2.b.SADD = curTxn->address << 1;  // Slave address to send
    CR2.b.RD_WRN = !tx;  // Transfer direction
    CR2.b.START = true;  // Signal start condition
    CR2.b.NBYTES = totallen;  // Total number of bytes until next START/STOP
    regs->CR2.d32 = CR2.d32;
}

void I2CBus::irqHandler()
{
    volatile STM32_I2C_REG_TYPE* regs = &STM32_I2C_REGS(index);
    union STM32_I2C_REG_TYPE::ISR ISR = { regs->ISR.d32 };
    
    // Handle various error conditions
    if (ISR.b.NAKF)  // NAK received
        error = ::I2C::RESULT_NAK;
    else if (ISR.b.TIMEOUT)  // Clock stretch timeout
        error = ::I2C::RESULT_TIMEOUT;
    else if (ISR.b.ARLO)  // Arbitration lost
    {
        error = ::I2C::RESULT_COLLISION;
        regs->CR1.d32 = 0;  // This event doesn't automatically cancel the transfer
    }
    else if (ISR.b.BERR)  // Bus error (START/STOP detected in the middle of a data byte)
        error = ::I2C::RESULT_UNKNOWN_ERROR;
    else if (ISR.b.OVR || ISR.b.PECERR)  // Buffer overrun or parity error
    {
        // The bus is in a good state but data was lost.
        // Cancel the transfer gracefully by sending a STOP condition.
        error = ::I2C::RESULT_UNKNOWN_ERROR;
        union STM32_I2C_REG_TYPE::CR2 CR2 = { 0 };
        CR2.b.STOP = true;
        regs->CR2.d32 = CR2.d32;
    }
    else if (ISR.b.STOPF && (totallen || !last) && error == ::I2C::RESULT_OK)  // Spurious STOP
        error = ::I2C::RESULT_UNKNOWN_ERROR;

    // Non-error events
    else if (ISR.b.RXNE)  // Data byte received
    {
        advanceIfNecessary();
        *buf++ = regs->RXDR;
        len--;
        totallen--;
    }
    else if (ISR.b.TXIS)  // Data for transmission needed
    {
        advanceIfNecessary();
        regs->TXDR = *buf++;
        len--;
        totallen--;
    }
    else if (ISR.b.TC)  // Transfer completed
    {
        // If this isn't the last transfer of the transaction, send a repeated START condition.
        if (!last) start();
        else
        {
            // Otherwise, send a STOP condition.
            union STM32_I2C_REG_TYPE::CR2 CR2 = { 0 };
            CR2.b.STOP = true;
            regs->CR2.d32 = CR2.d32;
        }
    }
    
    // Clear the IRQ
    regs->ICR.d32 = ISR.d32;
    
    if (!regs->ISR.b.BUSY)
    {
        // We have finished processing the transaction.
        // Wake up the task blocking on it and disable the I2C peripheral.
        busy = false;
        regs->CR1.d32 = 0;
        IRQ::wakeSensorTask();
    }
}

I2CBus I2CBus::I2C1(0);
I2CBus I2CBus::I2C2(1);
