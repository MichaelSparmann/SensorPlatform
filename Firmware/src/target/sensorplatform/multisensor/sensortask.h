#pragma once

// Sensor node sensor handling logic
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
#include "../common/protocol/rfproto.h"


namespace SensorTask
{
    enum State
    {
        State_Idle = 0,  // Sleeping, initial state
        State_PowerUp,  // Sensor power was just turned on, hardware needs to be initialized
        State_DetectSensors,  // Sensors are being detected and configuration is being verifierd
        State_WriteSensorPage,  // Sensor config is being changed and verified
        State_Measuring,  // Measurement is in progress
        State_Upgrading,  // Firmware upgrade has been triggered
    };

    // Sensor task schedule entries
    struct __attribute__((packed,aligned(4))) ScheduledTask
    {
        ScheduledTask* next;  // Pointer to next ScheduledTask in list
        int time;  // usec time that this ScheduledTask should be run at
        void (*call)(void* arg);  // Function to be called
        void* arg;  // Function argument

        constexpr ScheduledTask(void (*call)(void* arg), void* arg): next(NULL), time(0), call(call), arg(arg) {}
    };

    extern State state;
    extern uint32_t writeSeq;
    extern uint32_t endTime;
    extern uint64_t endOffset;

    extern void init();
    extern void detectSensors();
    extern RF::Result writeSensorPage(int pageid, void* data);
    extern void startMeasurement(int atTime);
    extern void stopMeasurement();
    extern void sleepUntil(int time);
    extern void scheduleTask(ScheduledTask* task);
    extern void writeMeasurement(uint16_t data);
    extern void yield();
}
