// Sensor node SD card driver
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
#include "sd.h"
#include "soc/stm32/gpio.h"
#include "soc/stm32/exti.h"
#include "cpu/arm/cortexm/cortexutil.h"
#include "sys/util.h"
#include "sys/time.h"
#include "driver/spi.h"
#include "driver/dma.h"
#include "driver/clock.h"
#include "common.h"
#include "storagetask.h"


#define SD_DMA_RX_REGS STM32_DMA_STREAM_REGS(SD_DMA_RX_CONTROLLER, SD_DMA_RX_STREAM)
#define SD_DMA_TX_REGS STM32_DMA_STREAM_REGS(SD_DMA_TX_CONTROLLER, SD_DMA_TX_STREAM)


namespace SD
{
    static const DMA::Config dmaRxCfg(SD_DMA_RX_PRIORITY, DMA::DIR_P2M, false,
                                      DMA::TS_8BIT, true, DMA::TS_8BIT, false, true);
    static const DMA::Config dmaTxCfg(SD_DMA_TX_PRIORITY, DMA::DIR_M2P, false,
                                      DMA::TS_8BIT, true, DMA::TS_8BIT, false, true);
    static const DMA::Config dummyTxCfg(SD_DMA_TX_PRIORITY, DMA::DIR_M2P, false,
                                        DMA::TS_8BIT, false, DMA::TS_8BIT, false, false);
    static const DMA::Config pollTxCfg(SD_DMA_TX_PRIORITY, DMA::DIR_M2P, true,
                                       DMA::TS_8BIT, false, DMA::TS_8BIT, false, true);
    static int8_t dummyTx = -1;


    static bool initialized;  // Whether the SD card was initialized since the last power cycle
    static bool sdhc;  // Whether the SD card is in SDHC (block addressing) mode

    uint32_t pageCount;  // SD card capacity (in 512-byte sectors)
    bool misoEdge;  // Whether a rising edge on the MISO line was detected (by EXTI)
                    // while waiting for the card to leave busy state


    // SD protocol CRC7 checksum generator (unused)
    static uint8_t crc7(uint8_t crc, uint8_t data)
    {
        for (int b = 0; b < 8; b++)
        {
            if ((crc ^ data) & 0x80) crc ^= 0x09;
            data <<= 1;
            crc <<= 1;
        }
        return crc;
    }

    // Wait for SD card to leave busy state (run from storage task)
    static bool waitIdle()
    {
        // Wait for any SPI transmissions to complete
        SPI::waitDone(&SD_SPI_BUS);
        // Clear MISO pin edge detector IRQ
        STM32::EXTI::clearPending(PIN_SD_MISO);
        // If MISO is already high, the card isn't in busy state
        if (GPIO::getLevel(PIN_SD_MISO)) return true;
        // If we get here, we will definitely get an EXTI IRQ once the card is idle.
        // Wait for a maximum of 500ms (standard specifies 250ms) for that to happen.
        long timeout = TIMEOUT_SETUP(500000);
        // Reset event flag and enable MISO pin edge detector
        misoEdge = false;
        STM32::EXTI::enableIRQ(PIN_SD_MISO, true);
        // Start an infinite dummy DMA transfer to clock the card
        DMA::startTransferWithLock(&SD_DMA_TX_REGS, pollTxCfg, &dummyTx, 65535);
        // Wait for MISO to go high
        while (!misoEdge && !TIMEOUT_EXPIRED(timeout)) StorageTask::yield();
        // The MISO edge detector is already disabled by its IRQ handler (in irq.cpp)
        // Cancel the dummy DMA transfer and wait for any remaining bytes to drain.
        DMA::clearIRQWithLock(SD_DMA_TX_CONTROLLER, SD_DMA_TX_STREAM);
        SPI::waitDone(&SD_SPI_BUS);
        // If an edge was detected by now, this isn't a timeout, so return success.
        return misoEdge;
    }

    // Send a command (ACMD if bit 7 set) to the SD card (run from storage task)
    static int sendCmd(uint8_t cmd, uint32_t arg)
    {
        if (cmd & 0x80)
        {
            // If this is an ACMD, prefix it with CMD55.
            cmd &= 0x7f;
            int rc = sendCmd(55, 0);
            if (rc < 0) return rc;
        }

        // Some (buggy) cards need extra clock cycles before every command
        SPI::pushByte(&SD_SPI_BUS, 0xff);
        // Wait for the card to be ready
        if (!waitIdle()) return -1;

        // Transmit command, argument and (dummy) CRC
        SPI::pushByte(&SD_SPI_BUS, 0x40 | cmd);
        SPI::pushByte(&SD_SPI_BUS, arg >> 24);
        SPI::pushByte(&SD_SPI_BUS, arg >> 16);
        SPI::pushByte(&SD_SPI_BUS, arg >> 8);
        SPI::pushByte(&SD_SPI_BUS, arg);
        uint8_t crcbyte = 1;
        if (cmd == 0) crcbyte = 0x95;
        else if (cmd == 8 && arg == 0x1aa) crcbyte = 0x87;
        SPI::pushByte(&SD_SPI_BUS, crcbyte);

        // Wait for card response. For STOP_TRANSMISSION commands, send another dummy byte.
        int tries = 10;
        if (cmd == 12) SPI::pushByte(&SD_SPI_BUS, 0xff);
        SPI::waitDone(&SD_SPI_BUS);
        uint8_t data = 0xff;
        while (tries-- && (data & 0x80)) data = SPI::xferByte(&SD_SPI_BUS, 0xff);
        // Check for timeout
        if (data & 0x80) return -2;
        // Return response byte
        return data;
    }

    // Read R3 response from card
    static uint32_t readR3()
    {
        int bytes = 4;
        uint32_t data = 0;
        while (bytes--) data = (data << 8) | SPI::xferByte(&SD_SPI_BUS, 0xff);
        return data;
    }

    // Read data block from card (run from storage task)
    static bool readBlock(void* buf, int len)
    {
        // Wait for data token (for up to 100ms)
        uint8_t data = 0xff;
        long timeout = TIMEOUT_SETUP(100000);
        while (!TIMEOUT_EXPIRED(timeout) && data == 0xff) data = SPI::xferByte(&SD_SPI_BUS, 0xff);
        if (data != 0xfe) return false;
        // Initiate DMA transfer
        DMA::startTransferWithLock(&SD_DMA_RX_REGS, dmaRxCfg, buf, len);
        DMA::startTransferWithLock(&SD_DMA_TX_REGS, dummyTxCfg, &dummyTx, len);
        // Wait for DMA completion
        while (!DMA::checkIRQ(SD_DMA_RX_CONTROLLER, SD_DMA_RX_STREAM)) StorageTask::yield();
        DMA::clearIRQWithLock(SD_DMA_RX_CONTROLLER, SD_DMA_RX_STREAM);
        DMA::clearIRQWithLock(SD_DMA_TX_CONTROLLER, SD_DMA_TX_STREAM);
        // Discard CRC bytes
        SPI::pushByte(&SD_SPI_BUS, 0xff);
        SPI::pushByte(&SD_SPI_BUS, 0xff);
        SPI::waitDone(&SD_SPI_BUS);
        return true;
    }

    // Send data block to card (run from storage task)
    static bool writeBlock(void* buf, int len, uint8_t token)
    {
        // Dummy byte required by standard
        SPI::pushByte(&SD_SPI_BUS, 0xff);
        // Wait for card to be ready
        if (!waitIdle()) return false;
        // Transmit data token
        SPI::pushByte(&SD_SPI_BUS, token);
        if (len)
        {
            // Initiate DMA transfer
            DMA::startTransferWithLock(&SD_DMA_TX_REGS, dmaTxCfg, buf, len);
            // Wait for DMA transfer to complete
            while (!DMA::checkIRQ(SD_DMA_TX_CONTROLLER, SD_DMA_TX_STREAM)) StorageTask::yield();
            DMA::clearIRQWithLock(SD_DMA_TX_CONTROLLER, SD_DMA_TX_STREAM);
            // Transmit dummy CRC
            SPI::pushByte(&SD_SPI_BUS, 0xff);
            SPI::pushByte(&SD_SPI_BUS, 0xff);
        }
        SPI::waitDone(&SD_SPI_BUS);
        // Check the card's data response
        if (len && (SPI::xferByte(&SD_SPI_BUS, 0xff) & 0x1f) != 5) return false;
        // Dummy byte to ensure that MISO line reflects busy state
        SPI::pushByte(&SD_SPI_BUS, 0xff);
        return true;
    }

    // Deselect the SD card
    static void finishCommand()
    {
        // Deassert NCS
        GPIO::setLevelFast(PIN_SD_NCS, true);
        // According to the standard, the card must stop driving MISO after the next byte.
        // However, some cards don't accept new commands if there aren't at least 32 clocks here.
        for (int i = 0; i < 4; i++) SPI::pushByte(&SD_SPI_BUS, 0);
        SPI::waitDone(&SD_SPI_BUS);
        // Turn off SPI interface clock gate
        Clock::offWithLock(SD_SPI_CLK);
    }

    // (Re-)initialize the SD card (run from storage task)
    void reset()
    {
        // Clear internal state
        initialized = false;
        sdhc = false;
        pageCount = 0;
        // Turn on SPI interface clock gate and set bus clock frequency for discovery mode (400kHz)
        Clock::onWithLock(SD_SPI_CLK);
        SPI::setFrequency(&SD_SPI_BUS, SD_SPI_DISCOVERY_PRESCALER);
        // Send 80 clock cycles (the standard requires at least 74, but we send multiples of 8)
        for (int i = 0; i < 10; i++) SPI::pushByte(&SD_SPI_BUS, 0xff);
        SPI::waitDone(&SD_SPI_BUS);
        // Select the card. Some cards seem to need an extra edge here for some reason.
        GPIO::setLevelFast(PIN_SD_NCS, false);
        GPIO::setLevelFast(PIN_SD_NCS, true);
        GPIO::setLevelFast(PIN_SD_NCS, false);
        // CMD0: GO_IDLE_STATE
        if (sendCmd(0, 0) != 1)
        {
            Clock::offWithLock(SD_SPI_CLK);
            error(Error_SDCMD0);
        }
        // CMD8: SEND_IF_COND in SD version 2 mode
        if (sendCmd(8, 0x1aa) == 1)
        {
            uint32_t ocr = readR3();
            if (ocr == 0x1aa)
            {
                // SD version 2 card: Attempt ACMD41: APP_SEND_OP_COND with SDHC flag set
                long timeout = TIMEOUT_SETUP(1000000);
                int result = 1;
                while (!TIMEOUT_EXPIRED(timeout) && result == 1) result = sendCmd(0x80 | 41, 1 << 30);
                // CMD58: READ_OCR
                if (result == 0 && sendCmd(58, 0) == 0)
                {
                    uint32_t ocr = readR3();
                    // Check if the card is in block addressing (SDHC) mode
                    if (ocr & (1 << 30)) sdhc = true;
                    initialized = true;
                }
            }
        }
        if (!initialized)
        {
            // Something in the SD version 2 init code failed, attempt to initialize as version 1
            long timeout = TIMEOUT_SETUP(1000000);
            int result = 1;
            uint8_t cmd = 0x80 | 41;  // ACMD41: APP_SEND_OP_COND
            while (!TIMEOUT_EXPIRED(timeout))
            {
                result = sendCmd(cmd, 0);
                if (result == 0) break;  // Success? Done.
                if (result == 1) continue;  // Busy? Try again
                if (cmd == 1) break;  // Error? If we were trying ACMD41, try CMD1. Else bail out.
                cmd = 1;  // CMD1: SEND_OP_COND
            }
            // Check if init was successful and set block length to 512 bytes (CMD16: SET_BLOCKLEN)
            if (result == 0 && sendCmd(16, 512) == 0) initialized = true;
        }
        if (!initialized)
        {
            // We were unable to initialize the card. This means that we won't be able to access
            // our configuration data, including our node unique ID. Bail out.
            GPIO::setLevelFast(PIN_SD_NCS, true);
            Clock::offWithLock(SD_SPI_CLK);
            error(Error_SDInit);
        }
        // Initialization completed. Switch to fast bus clock frequency.
        SPI::setFrequency(&SD_SPI_BUS, SD_SPI_PRESCALER);
        // Determine card capacity (CMD9: SEND_CSD)
        uint8_t csd[16];
        if (sendCmd(9, 0) == 0 && readBlock(csd, 16) && (csd[0] >> 7) == 0)
        {
            // See standard for an explanation of this mess.
            if ((csd[0] >> 6) == 1) pageCount = (((csd[7] & 0x3f) << 16) + (csd[8] << 8) + csd[9] + 1) << 10;
            else pageCount = (((csd[6] & 3) << 10) + (csd[7] << 2) + (csd[8] >> 6) + 1)
                          << (((csd[9] & 3) << 1) + (csd[10] >> 7) + (csd[5] & 0xf) - 7);
        }
        else error(Error_SDGetCapacity);
        // Deselect the card and disable SPI interface
        finishCommand();
    }

    // Read sector(s) from the SD card (run from storage task)
    void read(uint32_t page, uint32_t len, void* buf)
    {
        // Initialize the card if necessary
        if (!initialized) reset();
        // Check bounds
        if (page >= pageCount || len > pageCount - page) error(Error_SDReadOutOfBounds);
        if (!len) return;
        // If not in block addressing mode, multiply address by block size
        if (!sdhc) page *= 512;
        // Enable SPI interface and select card
        Clock::onWithLock(SD_SPI_CLK);
        GPIO::setLevelFast(PIN_SD_NCS, false);
        if (len == 1)
        {
            // Single-block case: Read a block using CMD17: READ_SINGLE_BLOCK
            if (sendCmd(17, page) != 0) error(Error_SDReadSingleCmd);
            if (!readBlock(buf, 512)) error(Error_SDReadSingleXfer);
        }
        else
        {
            // Multiple-block case: Initiate transfer using CMD18: READ_MULTIPLE_BLOCK
            if (sendCmd(18, page) != 0) error(Error_SDReadMultipleCmd);
            while (len--)
            {
                // Stream as many blocks as we want
                if (!readBlock(buf, 512)) error(Error_SDReadMultipleXfer);
                buf = (void*)(((uintptr_t)buf) + 512);
            }
            // Stop the transfer using CMD12: STOP_TRANSMISSION
            if (sendCmd(12, 0) != 0) error(Error_SDReadMultipleEnd);
        }
        // Deselect the card and disable SPI interface
        finishCommand();
    }

    // Write sector(s) to the SD card (run from storage task)
    void write(uint32_t page, uint32_t len, void* buf)
    {
        // Initialize the card if necessary
        if (!initialized) reset();
        // Check bounds
        if (page >= pageCount || len > pageCount - page) error(Error_SDWriteOutOfBounds);
        if (!len) return;
        // If not in block addressing mode, multiply address by block size
        if (!sdhc) page *= 512;
        // Enable SPI interface and select card
        Clock::onWithLock(SD_SPI_CLK);
        GPIO::setLevelFast(PIN_SD_NCS, false);
        if (len == 1)
        {
            // Single-block case: Write a block using CMD24: WRITE_BLOCK
            if (sendCmd(24, page) != 0) error(Error_SDWriteSingleCmd);
            if (!writeBlock(buf, 512, 0xfe)) error(Error_SDWriteSingleXfer);
        }
        else
        {
            // Multiple-block case: Pre-erase using ACMD23: SET_WR_BLOCK_ERASE_COUNT
            if (sendCmd(0x80 | 23, len) != 0) error(Error_SDWritePreErase);
            // Initiate transfer using CMD25: WRITE_MULTIPLE_BLOCK
            if (sendCmd(25, page) != 0) error(Error_SDWriteMultipleCmd);
            while (len--)
            {
                // Write as many blocks as we want
                if (!writeBlock(buf, 512, 0xfc)) error(Error_SDWriteMultipleXfer);
                buf = (void*)(((uintptr_t)buf) + 512);
            }
            // Stop the transfer using a STOP_TRANSMISSION token
            if (!writeBlock(NULL, 0, 0xfd)) error(Error_SDWriteMultipleEnd);
        }
        // Check for successful completion using CMD13: SEND_STATUS
        if (sendCmd(13, 0) != 0 || SPI::xferByte(&SD_SPI_BUS, 0xff) != 0) error(Error_SDWriteStatus);
        // Deselect the card and disable SPI interface
        finishCommand();
    }

    // Initiate streaming write (run from storage task, for measurement recording)
    void startWrite(uint32_t page, uint32_t len)
    {
        // Initialize the card if necessary
        if (!initialized) reset();
        // Check bounds
        if (page >= pageCount || len > pageCount - page) error(Error_SDWriteOutOfBounds);
        // If not in block addressing mode, multiply address by block size
        if (!sdhc) page *= 512;
        // Enable SPI interface and select card
        Clock::onWithLock(SD_SPI_CLK);
        GPIO::setLevelFast(PIN_SD_NCS, false);
        // Pre-erase using ACMD23: SET_WR_BLOCK_ERASE_COUNT
        if (sendCmd(0x80 | 23, len) != 0) error(Error_SDWritePreErase);
        // Initiate transfer using CMD25: WRITE_MULTIPLE_BLOCK
        if (sendCmd(25, page) != 0) error(Error_SDWriteMultipleCmd);
    }

    // Write a block in streaming write mode (run from storage task, for measurement recording)
    void writeSector(void* buf)
    {
        if (!writeBlock(buf, 512, 0xfc)) error(Error_SDWriteMultipleXfer);
    }

    // Finish streaming write (run from storage task, for measurement recording)
    void endWrite()
    {
        // Stop the transfer using a STOP_TRANSMISSION token
        if (!writeBlock(NULL, 0, 0xfd)) error(Error_SDWriteMultipleEnd);
        // Check for successful completion using CMD13: SEND_STATUS
        if (sendCmd(13, 0) != 0 || SPI::xferByte(&SD_SPI_BUS, 0xff) != 0) error(Error_SDWriteStatus);
        // Deselect the card and disable SPI interface
        finishCommand();
    }

    // Erase sector(s) on the SD card (similar to SSD TRIM, run from storage task)
    void erase(uint32_t page, uint32_t len)
    {
        // Initialize the card if necessary
        if (!initialized) reset();
        // Check bounds
        if (page >= pageCount || len > pageCount - page) error(Error_SDEraseOutOfBounds);
        if (!len) return;
        // If not in block addressing mode, multiply address by block size
        if (!sdhc) page *= 512;
        // Enable SPI interface and select card
        Clock::onWithLock(SD_SPI_CLK);
        GPIO::setLevelFast(PIN_SD_NCS, false);
        // CMD32: ERASE_WR_BLK_START
        if (sendCmd(32, page) != 0) error(Error_SDEraseSetStart);
        // CMD33: ERASE_WR_BLK_END
        if (sendCmd(33, page + len - 1) != 0) error(Error_SDEraseSetEnd);
        // CMD38: ERASE
        if (sendCmd(38, 0) != 0) error(Error_SDEraseCmd);
        // Wait for operation to finish
        if (!waitIdle()) error(Error_SDEraseEnd);
        // Deselect the card and disable SPI interface
        finishCommand();
    }

    // Prepare firmware upgrade: Enable card access and calculate address
    uint32_t prepareUpgrade(uint32_t page)
    {
        // Initialize the card if necessary
        if (!initialized) reset();
        // If not in block addressing mode, multiply address by block size
        if (!sdhc) page *= 512;
        // Lockout any IRQs (will not be unlocked before firmware upgrade completes)
        enter_critical_section();
        // Enable SPI interface and select card
        Clock::onFromPri0(SD_SPI_CLK);
        Clock::onFromPri0(STM32_CRC_CLOCKGATE);  // Enable CRC engine as well for updater
        GPIO::setLevelFast(PIN_SD_NCS, false);
        // Return firmware image address on card
        return page;
    }

    // SD card driver one-time init (NOT run from storage task, may not communicate with the card!)
    void init()
    {
        // Initialize SPI interface
        Clock::onWithLock(SD_SPI_CLK);
        SPI::init(&SD_SPI_BUS);
        Clock::offWithLock(SD_SPI_CLK);

        // Set up DMA streams to access the SPI interface
        DMA::setPeripheralAddr(&SD_DMA_RX_REGS, SPI::getDataRegPtr(&SD_SPI_BUS));
        DMA::setPeripheralAddr(&SD_DMA_TX_REGS, SPI::getDataRegPtr(&SD_SPI_BUS));

        // Deassert card chip select pin
        GPIO::enableFast(PIN_SD_NCS, true);

        // Configure MISO edge detector
        STM32::EXTI::configure(PIN_SD_MISO, STM32::EXTI::Config(false, false, STM32::EXTI::EDGE_RISING));
    }

    // Track power cycles to ensure that we re-initialize the card when necessary
    void shutdown(bool beforePowerDown)
    {
        initialized &= !beforePowerDown;
    }

    void wakeup(bool afterPowerDown)
    {
    }

}
