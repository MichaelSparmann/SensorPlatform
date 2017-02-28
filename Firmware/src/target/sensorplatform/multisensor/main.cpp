// Sensor node firmware entry point
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
#include "cpu/arm/cortexm/cmsis.h"
#include "sys/time.h"
#include "sys/util.h"
#include "driver/clock.h"
#include "driver/dma.h"
#include "driver/random.h"
#include "common.h"
#include "power.h"
#include "irq.h"
#include "radio.h"
#include "usb.h"
#include "sd.h"
#include "sensortask.h"
#include "storagetask.h"
#include "sensor/imu.h"


int main()
{
    // Initialize power management, power up sensor and radio bus
    Power::init();

    // Start up oscillator trimming
    Clock::init();

    // Setup IRQ priorities
    IRQ::init();

    // Start up true random number generator
    Random::init();

    // Power up DMA controllers, enable DMA IRQs
    DMA::init();

    // Radio one-time initialization (hardware setup)
    Radio::init();

    // Initialize sensor and measurement subsystem
    SensorTask::init();

    // Initialize SD card storage subsystem and discover contents
    StorageTask::init();

    // Start up radio communication and try to join a channel
    Radio::startup();

    //usb.start();

    // Everything from here on will run in handler mode
    SCB->CCR = SCB_SCR_SLEEPONEXIT_Msk;  // Stop CPU clock while not in handler mode
    while (true) idle();  // Should never be run
}
