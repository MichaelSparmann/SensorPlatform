#pragma once

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


namespace IRQ
{
    enum DPCNumber
    {
        DPC_Invalid = -1,
#define DEFINE_DPC(name, vector) DPC_ ## name,
#include "dpc_defs.h"
#undef DEFINE_DPC
    };

    extern void init();
    extern void clearRadioTimerIRQ();
    extern void wakeSensorTask();
    extern void wakeStorageTask();
    extern void setPending(DPCNumber dpc);
}
