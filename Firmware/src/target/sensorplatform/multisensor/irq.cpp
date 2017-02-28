// Sensor node interrupt and priority management
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
#include "irq.h"
#include "cpu/arm/cortexm/irq.h"
#include "cpu/arm/cortexm/cmsis.h"
#include "soc/stm32/gpio.h"
#include "soc/stm32/exti.h"
#include "soc/stm32/f0/rtc_regs.h"
#include "sys/util.h"
#include "driver/dma.h"
#include "i2c.h"
#include "sd.h"
#include "radio.h"
#include "power.h"
#include "sensortask.h"
#include "storagetask.h"
#include "sensor/imu.h"


namespace IRQ
{
    // Deferred procedure call vectors
    static void (* const dpcHandler[])() =
    {
#define DEFINE_DPC(name, vector) vector,
#include "dpc_defs.h"
#undef DEFINE_DPC
    };

    // Deferred procedure call pending flags
    static bool dpcPending[ARRAYLEN(dpcHandler)];

    // Priority and IRQ setup
    void init()
    {
        // Radio interrupt priority class:
        // These IRQs preempt everything else, but not each other to avoid race conditions.
        // At the time when a timer IRQ arrives, none of the other IRQ handlers should be active.
        irq_set_priority(dma1_stream4_7_dma2_stream3_5_IRQn, 0);  // Radio RX and TX DMA
        irq_set_priority(exti4_15_IRQn, 0);  // Radio IRQ
        irq_set_priority(tim7_IRQn, 0);  // Radio timer

        // Measurement interrupt priority class:
        irq_set_priority(tim2_IRQn, 1);  // Sensor task
        irq_set_priority(i2c1_IRQn, 1);  // Internal I2C bus
        irq_set_priority(i2c2_IRQn, 1);  // External I2C bus
        irq_set_priority(adc_comp_IRQn, 1);  // ADC

        // Storage interrupt priority class:
        irq_set_priority(dma1_stream2_3_dma2_stream1_2_IRQn, 2);  // SD card RX and TX DMA, storage task
        irq_set_priority(rtc_IRQn, 2);  // RTC IRQ

        // Background task priority class:
        irq_set_priority(usb_IRQn, 3);  // USB IRQ
        irq_set_priority(PendSV_IRQn, 3);  // Deferred procedure calls

        // Enable always-on IRQ sources:
        irq_enable(exti4_15_IRQn, true);  // Radio IRQ, SD card idle IRQ
        irq_enable(tim7_IRQn, true);  // Radio timer
        irq_enable(dma1_stream4_7_dma2_stream3_5_IRQn, true);  // Radio RX and TX DMA
        irq_enable(dma1_stream2_3_dma2_stream1_2_IRQn, true);  // SD card RX and TX DMA, storage task
        irq_enable(tim2_IRQn, true);  // Sensor task
        irq_enable(adc_comp_IRQn, true);  // ADC
        irq_enable(i2c1_IRQn, true);  // Internal I2C bus
        irq_enable(i2c2_IRQn, true);  // Internal I2C bus
        irq_enable(rtc_IRQn, true);  // RTC
}

    void clearRadioTimerIRQ()
    {
        irq_clear_pending(tim7_IRQn);
    }

    void wakeSensorTask()
    {
        irq_set_pending(tim2_IRQn);
    }

    void wakeStorageTask()
    {
        irq_set_pending(dma1_stream2_3_dma2_stream1_2_IRQn);
    }

    void setPending(DPCNumber dpc)
    {
        dpcPending[dpc] = true;
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

extern "C" void dma1_stream4_7_dma2_stream3_5_irqhandler()  // Radio RX and TX DMA
{
    DMA::clearIRQFromPri0(0, 3);
    DMA::clearIRQFromPri0(0, 4);
    Radio::handleDMACompletion();
}

extern "C" void exti4_15_irqhandler()  // Radio IRQ, SD card idle IRQ
{
    if (STM32::EXTI::getPending(PIN_RADIO_NIRQ)) Radio::handleIRQ();
    if (STM32::EXTI::getPending(PIN_SD_MISO))
    {
        STM32::EXTI::enableIRQ(PIN_SD_MISO, false);
        STM32::EXTI::clearPending(PIN_SD_MISO);
        SD::misoEdge = true;
        IRQ::wakeStorageTask();
    }
}

extern "C" void tim7_irqhandler()  // Radio timer
{
    Radio::timerTick();
}

extern "C" void rtc_irqhandler()  // RTC wakeup from sleep
{
    STM32_RTC_REGS.ISR.d32 = 0;
    STM32::EXTI::clearPending(STM32::EXTI::SOURCE_RTC_WAKEUP);
}

extern "C" void i2c1_irqhandler()  // Internal I2C bus
{
    I2CBus::I2C1.irqHandler();
}

extern "C" void i2c2_irqhandler()  // External I2C bus
{
    I2CBus::I2C2.irqHandler();
}

extern "C" void PendSV_faulthandler()  // Deferred procedure calls
{
    for (uint32_t i = 0; i < ARRAYLEN(IRQ::dpcHandler); i++)
        if (IRQ::dpcPending[i])
        {
            IRQ::dpcPending[i] = false;
            IRQ::dpcHandler[i]();
        }
}

// Handle CPU faults somewhat gracefully (halt debugger if present, blink LED pattern, reboot)
extern "C" void NonMaskableInt_faulthandler()
{
    error(Error_CPUFaultNMI);
}

extern "C" __attribute__((used)) void hardFaultHandler()
{
    error(Error_CPUFaultHard);
}

extern "C" __attribute__((naked,noinline)) void HardFault_faulthandler()
{
    // If this was caused by a breakpoint instruction (and no debugger is attached),
    // just skip the instruction. Otherwise call the real hard fault handler.
    __asm__ volatile(" \
        ldr R0, [SP,#24] \n\
        ldrh R1, [R0] \n\
        ldrh R2, =0xBEFF \n\
        cmp R1, R2 \n\
        bne realHardFault \n\
        add R0, #2 \n\
        str R0, [SP,#24] \n\
        bx LR \n\
        realHardFault: \n\
        ldr R0, =hardFaultHandler \n\
        bx R0 \n\
        .ltorg \n\
    ");
}

extern "C" void MemoryManagement_faulthandler()
{
    error(Error_CPUFaultMMU);
}

extern "C" void BusFault_faulthandler()
{
    error(Error_CPUFaultBus);
}

extern "C" void UsageFault_faulthandler()
{
    error(Error_CPUFaultUsage);
}
