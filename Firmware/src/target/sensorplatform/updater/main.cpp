// SensorPlatform Node Firmware Updater
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

// This is run from an arbitrary RAM address by the main sensor node firmware.
// It reads the new firmware image from the microSD card and verifies the CRC.
// If that matches, it erases the firmware flash and programs it with the new
// firmware read from the microSD card (once again). It then reboots.
// This code needs to fit into 512 bytes, so we'll have to use some tricks.

#include "global.h"
#include "cpu/arm/cortexm/cmsis.h"
#include "soc/stm32/spi_regs.h"
#include "soc/stm32/crc32_regs.h"
#include "soc/stm32/flash_regs.h"
#include "soc/stm32/f0/rcc_regs.h"
#include "sys/util.h"


// Reboot the SoC (ASM to get rid of useless stack frame)
static void __attribute__((noreturn,naked)) reboot()
{
    asm volatile("\n\
        STR %1, [%0]\n\
    " :: "l"(&SCB->AIRCR), "l"(0x05fa0004));
}

// Size-optimized SPI bus access code: spiXfer transmits a byte and returns the response.
// spiRead is basically "return spiXfer(0xff);". Spurious gcc warning ahead.
static uint8_t __attribute__((naked,noinline)) spiXfer(uint32_t byte) asm("spiXfer");
static uint8_t __attribute__((naked,noinline)) spiRead() asm("spiRead");
static uint8_t spiRead()
{
    asm("\n\
        MOV r0, #0xff\n\
        spiXfer:\n\
        LDR r1, =" STRINGIFY(SD_SPI_BASEADDR) "\n\
        STRB r0, [r1,#0xc]\n\
        1:\n\
        LDR r0, [r1,#0x8]\n\
        LSL r0, r0, #0x1f\n\
        BPL 1b\n\
        LDRB r0, [r1,#0xc]\n\
        BX lr\n\
    " ::: "r0", "r1", "lr");
}

// Sends a command to the microSD card. If anything fails, reboot.
static void sdSendCmd(uint32_t arg, uint32_t cmd)
{
    // Wait for idle
    while (spiRead() != 0xff);
    // Transmit command + arg
    spiXfer(0x40 | cmd);
    spiXfer(arg >> 24);
    spiXfer(arg >> 16);
    spiXfer(arg >> 8);
    spiXfer(arg);
    // Transmit dummy CRC7 + stop bit
    spiXfer(1);
    // STOP_TRANSMISSION needs more clock cycles here
    if (cmd == 12) spiRead();
    // Wait for a response from the card
    int tries = 10;
    uint8_t data = 0xff;
    while (tries-- && (data & 0x80)) data = spiRead();
    // If the card reports an error, reboot
    if (data & 0x80) reboot();
}

// Main firmware upgrade logic. Size is in sectors.
extern "C" void __attribute__((naked,section(".startup"))) _startup(uint32_t startPage, uint32_t size, uint32_t crc)
{
    // Read the new firmware from the microSD card and check its CRC
    sdSendCmd(startPage, 18);  // READ_MULTIPLE_BLOCK
    int sectors = size;
    // Reset HW CRC32 engine
    union STM32_CRC_REG_TYPE::CR CRCCR = { 0 };
    CRCCR.b.RESET = true;
    STM32_CRC_REGS.CR.d32 = CRCCR.d32;
    do  // For each sector:
    {
        // Wait for data token
        uint8_t data = 0xff;
        while (data == 0xff) data = spiRead();
        // If the card reports an error, reboot
        if (data != 0xfe) reboot();
        int len = 128;
        do
        {
            // Read 32 bits and push them to the HW CRC32 engine
            // (gcc generates a huge mess here, so use ASM)
            uint32_t tmp;
            asm volatile("\n\
                BL spiRead\n\
                MOV %0, r0\n\
                BL spiRead\n\
                LSL r0, r0, #8\n\
                ORR %0, %0, r0\n\
                BL spiRead\n\
                LSL r0, r0, #16\n\
                ORR %0, %0, r0\n\
                BL spiRead\n\
                LSL r0, r0, #24\n\
                ORR %0, %0, r0\n\
                STR %0, [%1,#0]\n\
            " : "=l"(tmp) : "l"(&STM32_CRC_REGS) : "r0", "r1", "lr");
        } while (--len);
        // Consume CRC from the SD card
        spiRead();
        spiRead();
    } while (--sectors);
    sdSendCmd(0, 12);  // STOP_TRANSMISSION
    // Some buggy cards require some extra clock cycles here
    for (int i = 0; i < 4; i++) spiRead();
    // If the CRC32 check failed, reboot.
    if (STM32_CRC_REGS.DR != crc) reboot();

    // Unlock flash write access
    STM32_RCC_REGS.CR.b.HSION = true;
    STM32_FLASH_REGS.KEYR = 0x45670123;
    STM32_FLASH_REGS.KEYR = 0xcdef89ab;

    // Erase the firmware flash
    volatile uint16_t* flashPtr = (volatile uint16_t*)0x08000000;
    const void* flashEnd = (void*)(0x08000000 + size * 512);
    union STM32_FLASH_REG_TYPE::CR CR = { STM32_FLASH_REGS.CR.d32 };
    union STM32_FLASH_REG_TYPE::CR CRPER = { CR.d32 };
    CRPER.b.PER = true;
    STM32_FLASH_REGS.CR.d32 = CRPER.d32;
    CRPER.b.STRT = true;
    do  // For each firmware flash page:
    {
        asm volatile("\n\
            STR %0, [%2,#0x14]\n\
            STR %1, [%2,#0x10]\n\
            ADD %0, %0, %3\n\
            0:\n\
            LDR r0, [%2,#0xc]\n\
            LSL r0, r0, #0x1f\n\
            BMI 0b\n\
        " : "+l"(flashPtr) : "l"(CRPER.d32), "l"(&STM32_FLASH_REGS), "l"(FLASH_PAGESIZE / sizeof(*flashPtr)));
    } while (flashPtr < flashEnd);

    // Read the new firmware from the microSD card again and program it to the firmware flash
    flashPtr = (volatile uint16_t*)0x08000000;
    sdSendCmd(startPage, 18);  // READ_MULTIPLE_BLOCK
    CR.b.PG = true;
    STM32_FLASH_REGS.CR.d32 = CR.d32;
    do  // For each sector:
    {
        // Wait for data token
        uint8_t data = 0xff;
        while (data == 0xff) data = spiRead();
        // If the card reports an error, reboot
        if (data != 0xfe) reboot();
        int len = 256;
        do
        {
            // Read 16 bits and write them to the firmware flash
            uint16_t tmp;
            asm volatile("\n\
                BL spiRead\n\
                MOV %0, r0\n\
                BL spiRead\n\
                LSL r0, r0, #8\n\
                ORR %0, %0, r0\n\
                STRH %0, [%1]\n\
                ADD %1, %1, #2\n\
                0:\n\
                LDR r0, [%2,#0xc]\n\
                LSL r0, r0, #0x1f\n\
                BMI 0b\n\
            " : "=l"(tmp), "+l"(flashPtr) : "l"(&STM32_FLASH_REGS) : "r0", "r1", "lr");
        } while (--len);
        // Consume CRC from the SD card
        spiRead();
        spiRead();
    } while (flashPtr < flashEnd);

    // Reboot into the new firmware
    reboot();
}
