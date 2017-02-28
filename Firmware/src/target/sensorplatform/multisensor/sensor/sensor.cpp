// Sensor node generic sensor abstraction functions
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
#include "sensor.h"
#include "sys/util.h"


// Sensor detection / config load (if first is true, there was a power cycle since the last run)
void Sensor::init(bool first)
{
    SeriesHeader::SensorInfo* info = getInfoPtr();
    // If the sensor type referred to in the configuration doesn't match,
    // clear the configuration and pre-populate it with the correct type ID.
    if (info->info.vendor != type->vendorId || info->info.product != type->productId)
    {
        memset(info, 0, sizeof(*info));
        info->info.vendor = type->vendorId;
        info->info.product = type->productId;
    }
    // Run the sensor driver's initialization function if it has one.
    if (type->init) type->init(this, first);
    // Verify sensor configuration
    verifyConfig();
}

// Verify (and fix) sensor configuration information
void Sensor::verifyConfig()
{
    SeriesHeader::SensorInfo* info = getInfoPtr();
    if (!present)
    {
        // The sensor is not present. Clear sensor information.
        memset(info, 0, sizeof(*info));
        interval = 0;
        return;
    }
    // Run the sensor driver's configuration validation function if it has one.
    if (type->verify) type->verify(this, info);
}

// Apply a configuration change to a sensor if possible
bool Sensor::writePage(int page, Page* data)
{
    // Figure out which bytes of this page are writable
    uint32_t mask = type->writeMask[page] << 4;
    // If none are, or the sensor isn't even present, return failure.
    if (!present || !mask) return false;
    // Change any writable bytes to the requested value
    uint8_t* src = data->u8;
    uint8_t* dest = getInfoPtr()->page[page].u8;
    while (mask)
    {
        if (mask & 0x80000000) *dest = *src;
        src++;
        dest++;
        mask <<= 1;
    }
    // Verify sensor configuration
    verifyConfig();
    // Return success
    return true;
}

// Prepare the sensor for start of measurement
void Sensor::start(int time)
{
    // If this sensor isn't present, we don't need to do anything.
    if (!present) return;
    SeriesHeader::SensorInfo* info = getInfoPtr();
    // If the sensor is supposed to be samples, schedule captureTask for the first sample
    // and cache the interval (before the sensor configuration gets flushed from the buffer).
    interval = info->info.scheduleInterval;
    captureTask.time = time + info->info.scheduleOffset;
    if (interval) SensorTask::scheduleTask(&captureTask);
    // Run the sensor driver's measurement start function if it has one.
    if (type->start) type->start(this, captureTask.time);
}

// Shut down the sensor after measurement
void Sensor::stop()
{
    // If this sensor isn't present, we don't need to do anything.
    if (!present) return;
    // Run the sensor driver's measurement stop function if it has one.
    if (type->stop) type->stop(this);
}

// Convenience wrapper to schedule a given task at a given time
void Sensor::schedule(SensorTask::ScheduledTask* task, int time)
{
    task->time = time;
    SensorTask::scheduleTask(task);
}

// Convenience wrapper to re-schedule a given task exactly one sampling interval later
void Sensor::reschedule(SensorTask::ScheduledTask* task)
{
    task->time += interval;
    SensorTask::scheduleTask(task);
}
