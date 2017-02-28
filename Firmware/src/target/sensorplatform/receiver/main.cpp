// SensorPlatform Base Station Firmware Entry Point
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
#include "app/main.h"
#include "sys/time.h"
#include "sys/util.h"
#include "soc/stm32/gpio.h"
#include "driver/dma.h"
#include "driver/random.h"
#include "irq.h"
#include "radio.h"
#include "usb.h"


int main()
{
    // Setup IRQ priorities
    IRQ::init();

    // Start up true random number generator
    Random::init();

    // Power up DMA controllers, enable DMA IRQs
    DMA::init();

    // Keep all GPIO controllers awake all the time.
    // Power is not an issue on the receiver side, but performance is.
    // A lot of other code blindly relies on this, so don't touch it.
    GPIO::enableFast(PIN_A0, true);
    GPIO::enableFast(PIN_B0, true);
    GPIO::enableFast(PIN_C0, true);
    GPIO::enableFast(PIN_D0, true);

    // Radio one-time initialization (hardware setup)
    Radio::init();

    // Start up USB stack and message processing
    USB::start();

    // Everything from here on is IRQ driven
    while (true) idle();
}
