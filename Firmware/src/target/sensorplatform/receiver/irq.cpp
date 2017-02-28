// SensorPlatform Base Station Interrupt and Priority Management
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
#include "sys/util.h"
#include "driver/dma.h"
#include "radio.h"
#include "hub.h"


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
        irq_set_priority_grouping(0);

        // Radio interrupt priority class:
        // These IRQs preempt everything else, but not each other to avoid race conditions.
        // At the time when a timer IRQ arrives, none of the other IRQ handlers should be active.
        irq_set_priority(dma1_stream3_IRQn, 0);  // Radio RX DMA
        irq_set_priority(dma1_stream4_IRQn, 0);  // Radio TX DMA
        irq_set_priority(exti4_IRQn, 0);  // Radio IRQ
        irq_set_priority(tim7_IRQn, 0);  // Radio timer

        // Background task priority class:
        irq_set_priority(otg_hs_IRQn, 3);  // Main USB IRQ
        irq_set_priority(otg_fs_IRQn, 3);  // Aux USB IRQ
        irq_set_priority(PendSV_IRQn, 3);  // Deferred procedure calls
    }

    void enableRadioIRQ(bool on)
    {
        irq_enable(exti4_IRQn, on);
    }

    void enableRadioTimerIRQ(bool on)
    {
        irq_enable(tim7_IRQn, on);
    }

    void enableRadioDMAIRQs(bool on)
    {
        irq_enable(dma1_stream3_IRQn, on);
        irq_enable(dma1_stream4_IRQn, on);
    }

    void clearRadioTimerIRQ()
    {
        irq_clear_pending(tim7_IRQn);
    }

    void setPending(DPCNumber dpc)
    {
        dpcPending[dpc] = true;
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

extern "C" void dma1_stream3_irqhandler()  // Radio RX DMA
{
    DMA::clearIRQFromPri0(0, 3);
    Radio::handleRXDMACompletion();
}

extern "C" void dma1_stream4_irqhandler()  // Radio TX DMA
{
    DMA::clearIRQFromPri0(0, 4);
    Radio::handleTXDMACompletion();
}

extern "C" void exti4_irqhandler()  // Radio IRQ
{
    Radio::handleIRQ();
}

extern "C" void tim7_irqhandler()  // Radio timer
{
    Radio::timerTick();
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
