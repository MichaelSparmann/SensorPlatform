#pragma once

// Sensor node radio interface
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
#include "interface/gpio/gpio.h"
#include "device/nrf/nrf24l01p/nrf24l01p.h"
#include "../common/protocol/rfproto.h"


namespace Radio
{
    extern bool connected;
    extern uint8_t failedAssocAttempts;
    extern int globalTimeOffset;
    extern RF::Packet::Reply::NoData noDataResponse;
    extern bool measuring;
    extern bool seriesComplete;
    extern uint32_t bufferOverflowLost;

    extern void init();
    extern void shutdown();
    extern void startup();
    extern void handleDMACompletion();
    extern void handleIRQ();
    extern void timerTick();
    extern RF::Packet::Reply* getFreeTxBuffer(int reserveSlots);
    extern void enqueuePacket(int maxAttempts);
    extern void sharedSPITransfer(GPIO::Pin pin, uint8_t prescaler, const void* txBuf, void* rxBuf, uint8_t len);
    extern void startMeasurementTransmission();
    extern void dpcFrameTask();
    extern void dpcCommandHandler();
}
