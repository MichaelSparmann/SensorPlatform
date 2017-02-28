#pragma once

// Sensor node generic sensor abstraction interface
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
#include "../common.h"
#include "../sensortask.h"


class Sensor;

// Read-only sensor driver information.
// This is not part of the Sensor class to avoid it being placed in RAM
// (and being duplicated for multiple instances of the same sensor driver).
struct SensorType
{
    const uint32_t writeMask[4];  // Bit mask of writable sensor information bytes
    const uint32_t vendorId;  // Sensor hardware vendor ID
    const uint32_t productId;  // Sensor hardware product ID
    // Sensor driver vtable: (for Sensor objects, but stored in SensorType class)
    void (*const init)(Sensor* sensor, bool first);
    void (*const verify)(Sensor* sensor, SeriesHeader::SensorInfo* info);
    void (*const start)(Sensor* sensor, int time);
    void (*const stop)(Sensor* sensor);
};

// Sensor instance (in RAM)
class Sensor
{
public:
    const SensorType* const type;  // Pointer to constant data (attributes, vtable) in flash
    SensorTask::ScheduledTask captureTask;  // Primary sampling task
    uint32_t interval;  // captureTask rescheduling interval (copied from series header)
    const uint8_t id;  // Sensor ID of this instance
    bool present;  // Whether the sensor is physically present on this node

    constexpr Sensor(const SensorType* type, uint8_t id, void (*captureTask)(Sensor*))
        : type(type), captureTask((void(*)(void*))captureTask, this), interval(0), id(id), present(false) {}

    void init(bool first);  // needs to set present flag
    constexpr SeriesHeader::SensorInfo* getInfoPtr() { return mainBuf.seriesHeader.sensor + id; }
    bool writePage(int page, Page* data);
    void verifyConfig();
    void start(int time);
    void stop();
    void schedule(SensorTask::ScheduledTask* task, int time);
    void reschedule(SensorTask::ScheduledTask* task);
};
