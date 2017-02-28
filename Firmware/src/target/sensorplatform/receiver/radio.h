#pragma once

// SensorPlatform Base Station Radio Interface
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
#include "device/nrf/nrf24l01p/nrf24l01p.h"
#include "sys/util.h"
#include "../common/protocol/usbproto.h"


namespace Radio
{
    extern USB::RadioStats stats;
    extern RF::TimeSlotOwner nextPacketSlots[ARRAYLEN(RF::Packet::SOF::slot)];

    extern void init();
    extern void shutdown();
    extern void configure(RF::ExtendedChannelAttributes* channelAttrs);
    extern void handleRXDMACompletion();
    extern void handleTXDMACompletion();
    extern void handleIRQ();
    extern void timerTick();
    extern RF::Packet* getCommandBuffer();
    extern void enqueueCommand(uint8_t target);
    extern RF::Packet* getNextRxPacket();
    extern void releaseRxPacket();
}
