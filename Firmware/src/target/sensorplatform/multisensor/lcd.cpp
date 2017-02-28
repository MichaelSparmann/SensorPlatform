// LCD driver (for PCD8544-based LCDs) for sensor node breadboard setup.
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

// For debugging purposes only. Cannot be used on sensor node PCBs. Uses shared SPI with radio.

#include "global.h"
#include "lcd.h"
#include "sys/util.h"
#include "soc/stm32/gpio.h"
#include "lib/fonts/prop8/prop8.h"
#include "lib/printf/printf.h"
#include "radio.h"
#include "irq.h"
#include "../common/driver/timer.h"


namespace LCD
{
    static const Fonts::Prop8::Font* const font = &Fonts::Prop8::small[0];
    static uint8_t frameBuf[6][84];
    static uint16_t scanPtr;
    static uint16_t printPtr;

    static void sendCmd(bool ext, uint8_t cmd)
    {
        uint8_t buf[] = {(uint8_t)(0x20 | ext), cmd, 0x20};
        GPIO::setLevel(PIN_LCD_CD, false);
        Radio::sharedSPITransfer(PIN_LCD_CE, LCD_SPI_PRESCALER, buf, NULL, sizeof(buf));
        GPIO::setLevel(PIN_LCD_CD, true);
    }

    static void jump(int row, int col)
    {
        uint8_t buf[] = {(uint8_t)(0x40 | row), (uint8_t)(0x80 | col)};
        GPIO::setLevel(PIN_LCD_CD, false);
        Radio::sharedSPITransfer(PIN_LCD_CE, LCD_SPI_PRESCALER, buf, NULL, sizeof(buf));
        GPIO::setLevel(PIN_LCD_CD, true);
    }

    void init()
    {
        GPIO::enableFast(PIN_LCD_CE, true);
        setBias(LCD_BIAS);
        setTc(LCD_TC);
        setVop(LCD_VOP);
        blank(false, false);
        jump(0, 0);
        Timer::start(&LCD_TIMER, LCD_TIMER_CLK, 48, 1000);
        IRQ::enableLCDTimerIRQ(true);
    }

    void powerDown()
    {
        sendCmd(true, 0x24);
    }

    void blank(bool blank, bool invert)
    {
        sendCmd(false, 0x08 | ((!blank) << 2) | (!!invert));
    }

    void setVop(int vop)
    {
        sendCmd(true, 0x80 | vop);
    }

    void setBias(int bias)
    {
        sendCmd(true, 0x10 | bias);
    }

    void setTc(int tc)
    {
        sendCmd(true, 0x04 | tc);
    }

    void clear()
    {
        memset(*frameBuf, 0, sizeof(frameBuf));
    }

    uint8_t* drawChar(uint8_t* ptr, char c)
    {
        const Fonts::Prop8::Font* fp = font;
        while (fp[1] && (fp[0] > c || fp[1] < c)) fp += 4 + fp[1] - fp[0] + fp[fp[1] - fp[0] + 3];
        if (!fp[1]) return ptr;
        int width = fp[3 + c - fp[0]] - fp[2 + c - fp[0]];
        fp += fp[1] - fp[0] + 4 + fp[2 + c - fp[0]];
        while (width--) *ptr++ = *fp++;
        *ptr++ = 0;
        return ptr;
    }

    int putch(int row, int col, char c)
    {
        return drawChar(&frameBuf[row][col], c) - frameBuf[row];
    }

    int print(int row, int col, const char* text)
    {
        if (!text || !*text) return col;
        uint8_t* ptr = &frameBuf[row][col];
        char c;
        while ((c = *text++)) ptr = drawChar(ptr, c);
        return ptr - frameBuf[row];
    }

    int printfChar(void* state, uint8_t data)
    {
        printPtr = drawChar((*frameBuf) + printPtr, data) - *frameBuf;
        return true;
    }

    int printf(int row, int col, const char* format, ...)
    {
        if (!format || !*format) return col;
        va_list args;
        va_start(args, format);
        printPtr = &frameBuf[row][col] - *frameBuf;
        printf_format(&printfChar, NULL, format, args);
        va_end(args);
        return *frameBuf + printPtr - frameBuf[row];
    }

    int blit(int row, int col, const uint8_t* data, int len)
    {
        memcpy(&frameBuf[row][col], data, len);
        return col + len;
    }

    void timerTick()
    {
        Timer::acknowledgeIRQ(&LCD_TIMER);
        int len = MIN(sizeof(frameBuf) - scanPtr, 16);
        Radio::sharedSPITransfer(PIN_LCD_CE, LCD_SPI_PRESCALER, (*frameBuf) + scanPtr, NULL, len);
        scanPtr += len;
        if (scanPtr >= sizeof(frameBuf)) scanPtr = 0;
    }
}
