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
#include "sensortask.h"
#include "cpu/arm/cortexm/cortexutil.h"
#include "sys/time.h"
#include "sys/util.h"
#include "../common/driver/timer.h"
#include "irq.h"
#include "radio.h"
#include "i2c.h"
#include "storagetask.h"
#include "sensor/sensor.h"
#include "sensor/timing.h"
#include "sensor/telemetry.h"
#include "sensor/imu.h"
#include "sensor/baro.h"
#include "sensor/hygro.h"
#include "sensor/light.h"


namespace SensorTask
{
    // Map of sensor objects (for all possible sensor IDs)
    static Sensor* const sensors[ARRAYLEN(mainBuf.seriesHeader.sensor)] =
    {
#define DEFINE_SENSOR(name, obj) &obj,
#include "sensor_defs.h"
#undef DEFINE_SENSOR
    };

    static bool initialized = false;  // Whether sensor hardware is already initialized
    static bool stop;  // Whether the running measurement was requested to be stopped
    static uint32_t cmdArg;  // Argument of a pending command (usually an index or timestamp)
    static void* cmdPtr;  // Argument of a pending command (usually a pointer)
    static ScheduledTask* nextTask;  // First entry in ScheduledTask queue
    static uint8_t writeBlock;  // Measurement data recording buffer block pointer
    static uint8_t writeWord;  // Word (16 bit) pointer within writeBlock

    State state = State_Idle;  // Requested or running operation
    uint32_t writeSeq;  // Sequence number of the current measurement data block
    uint32_t endTime;  // Length (in usec) of the last completed measurement (may wrap around)
    uint64_t endOffset;  // Length (in bytes) of the last completed measurement


    // Sleep until a certain usec time is reached (called from sensor task)
    void sleepUntil(int time)
    {
        // While the target time has not been reached
        int now = read_usec_timer();
        while (TIME_BEFORE(now, time))
        {
            // If there are more than 5us left
            if (time - now > 5)
            {
                // Request wakeup from hardware timer
                Timer::scheduleIRQ(&TICK_TIMER, time - 1);
                // If the target time still hasn't passed, leave sensor task
                if (!TIMEOUT_EXPIRED(time - 1)) yield();
            }
            now = read_usec_timer();
        }
    }

    // Insert a ScheduledTask into the queue (called from sensor task in Measuring state)
    void scheduleTask(ScheduledTask* task)
    {
        ScheduledTask* prev;
        ScheduledTask* t;
        // Walk ScheduledTask list to find correct insertion point (ordered by target time)
        for (prev = NULL, t = nextTask; t; prev = t, t = t->next)
            if (TIME_AFTER(t->time, task->time))
                break;
        // Insert the new task
        task->next = t;
        if (prev) prev->next = task;
        else nextTask = task;
    }

    // Write a word into the measurement data buffer (called from sensor task in Measuring state)
    void writeMeasurement(uint16_t data)
    {
        // Write the word
        Page* block = mainBuf.block[writeBlock];
        block->u16[writeWord++] = data;
        if (writeWord >= sizeof(*mainBuf.block) / sizeof(*block->u16))
        {
            // We have just filled up a block, so move on to the next one.
            writeWord = 0;
            mainBufSeq[writeBlock] = writeSeq++;
            mainBufValid[writeBlock] = true;
            // Increment block pointer, wrap around if necessary
            if (++writeBlock >= ARRAYLEN(mainBuf.block)) writeBlock = 0;
            mainBufValid[writeBlock] = false;
            // Wake storage task (it may need to write the just completed block to the SD card)
            IRQ::wakeStorageTask();
        }
    }

    // Detect present sensors and validate configuration in series header (called externally)
    void detectSensors()
    {
        // Check if the sensor task is able to accept the request
        if (state != State_Idle && state != State_PowerUp) error(Error_SensorDetectNotIdle);
        // Signal request and wake up sensor task
        state = State_DetectSensors;
        IRQ::wakeSensorTask();
    }

    // Change sensor configuration page (called externally)
    RF::Result writeSensorPage(int pageid, void* data)
    {
        // Check if the sensor task is able to accept the request
        if (state != State_Idle) return RF::Result_Busy;
        // Signal request and wake up sensor task
        state = State_WriteSensorPage;
        cmdArg = pageid;
        cmdPtr = data;
        IRQ::wakeSensorTask();
        // Wait for competion and return result (passed back in cmdArg)
        while (state != State_Idle) WFE();
        if (cmdArg) return RF::Result_OK;
        else return RF::Result_InvalidArgument;
    }

    // Initiate measurement (called externally)
    void startMeasurement(int atTime)
    {
        // Check if the sensor task is able to accept the request
        if (state != State_Idle) error(Error_SensorStartMeasurementNotIdle);
        // Signal request and wake up sensor task
        state = State_Measuring;
        cmdArg = atTime;
        IRQ::wakeSensorTask();
    }

    // Stop a running measurement (called externally)
    void stopMeasurement()
    {
        // Ignore this if no measurement is running (might be a command retransmission)
        if (state != State_Measuring) return;
        // Signal to sensor task that it should stop the measurement and wake it up
        stop = true;
        IRQ::wakeSensorTask();
        // Wait for the sensor task to acknowledge the stop request
        while (stop) WFE();
    }

    // Sensor task entry point (initially in Idle state)
    static void run()
    {
        // Configure I2C bus
        I2CBus::I2C1.init();

        // Sensor task main loop
        while (true)
        {
            switch (state)
            {
            case State_PowerUp:
                // Sensors have just been powered up and will have to be re-initialized
                initialized = false;
                // Wait for sensors to be able to communicate (IMU is slowest)
                sleepUntil(IMU::bootedAt);
                // Put the IMU into sleep mode
                IMU::powerDown();
                if (state == State_PowerUp) state = State_Idle;
                break;

            case State_DetectSensors:
                // Attempt to initialize all sensors
                for (uint32_t i = 0; i < ARRAYLEN(sensors); i++)
                {
                    // If a sensor isn't present, clear its configuration data in the series header
                    if (sensors[i]) sensors[i]->init(!initialized);
                    else memset(mainBuf.seriesHeader.sensor + i, 0, sizeof(*mainBuf.seriesHeader.sensor));
                }
                // Series header might potentially have been modified during sensor detection
                StorageTask::seriesHeaderDirty = true;
                initialized = true;
                // Signal completion
                state = State_Idle;
                IRQ::setPending(IRQ::DPC_RadioCommandHandler);
                break;

            case State_WriteSensorPage:
                // Pass request to sensor driver
                cmdArg = sensors[cmdArg >> 2]->writePage(cmdArg & 3, (Page*)cmdPtr);
                // Series header might potentially have been modified
                StorageTask::seriesHeaderDirty = true;
                // Signal completion
                state = State_Idle;
                SEV();
                break;

            case State_Measuring:
                // Clear measurement state information
                stop = false;
                memset(mainBufSeq, 0, sizeof(mainBufSeq));
                memset(mainBufValid, 0, sizeof(mainBufValid));
                // Calculate number of series header blocks (16)
                writeBlock = sizeof(mainBuf.seriesHeader) / sizeof(*mainBuf.block);
                // Mark series header blocks as ready to be recorded.
                // (Their content is in the measurement buffer while in Idle state)
                for (writeSeq = 0; writeSeq < writeBlock; writeSeq++)
                {
                    mainBufSeq[writeSeq] = writeSeq;
                    mainBufValid[writeSeq] = true;
                }
                // If the series header fills the measurement data buffer, wrap around
                if (writeBlock >= ARRAYLEN(mainBuf.block)) writeBlock = 0;
                writeWord = 0;
                nextTask = NULL;
                // Start up sensors (cmdArg is usec time to start measuring at)
                for (uint32_t i = 0; i < ARRAYLEN(sensors); i++)
                    if (sensors[i])
                        sensors[i]->start(cmdArg);
                // Measurement main loop
                while (!stop)
                {
                    // Determine the ScheduledTask that needs to be executed next
                    ScheduledTask* task = nextTask;
                    if (!task)
                    {
                        // The ScheduledTask list is empty (no sensors active?)
                        yield();
                        continue;
                    }
                    nextTask = task->next;
                    // Sleep until the target execution time of the next ScheduledTask
                    sleepUntil(task->time);
                    // Run the task (it will re-schedule itself if it needs to)
                    task->call(task->arg);
                }
                // Measurement has ended, figure out the usec time (within measurement schedule)
                // that the next sensor would have been sampled at and report this as end time.
                if (nextTask) endTime = nextTask->time - cmdArg;
                else endTime = 0;
                // Figure out how many bytes of measurement data we have captured.
                endOffset = ((uint64_t)writeSeq) * sizeof(*mainBuf.block) + writeWord * sizeof(*(*mainBuf.block)->u16);
                // Acknowledge stop request
                stop = false;
                SEV();
                // Fill up current measurement data block with zeros, to allow for it to be written
                while (writeWord) writeMeasurement(0);
                // Shut down sensors
                for (uint32_t i = ARRAYLEN(sensors); i--; )
                    if (sensors[i])
                        sensors[i]->stop();
                // No more data will be written, signal measurement completion
                Radio::seriesComplete = true;
                // Wait for radio measurement data transmission to complete.
                // (Triggering LoadSeriesHeader below will overwrite the measurement data buffer.)
                while (Radio::measuring) yield();
                // Go back to idle state (the storage task will move us to DetectSensors later)
                state = State_Idle;
                // Tell the storage task to re-load the series header to the measurement data
                // buffer once it has finished writing measurement data. Wake it up in case it
                // did already finish. The storage task will call detectSensors() after re-loading
                // the series header (which contains sensor configuration) from the SD card.
                StorageTask::state = StorageTask::State_LoadSeriesHeader;
                IRQ::wakeStorageTask();
                break;

            case State_Upgrading:
                // Firmware upgrade has been triggered, but needs to send an acknowledgement.
                // Allow 100ms of time for that, then wake up the storage task,
                // which will then complete the upgrade and reboot.
                sleepUntil(read_usec_timer() + 100000);
                state = State_Idle;
                IRQ::wakeStorageTask();
                break;

            case State_Idle:
                yield();
                break;

            default:
                error(Error_SensorMainLoopInvalidState);
            }
        }
    }

    // Sensor task stack. Contains any stack frames of the storage task
    // plus those layered on top by radio IRQ handling.
    static uint32_t __attribute__((section(".stack"))) taskStack[128];
    // Current sensor task stack pointer value (if the sensor task is not currently running)
    // or stack pointer to return to when yielding (if the sensor task is running)
    void* __attribute__((used)) taskStackPtr = taskStack + ARRAYLEN(taskStack) - 9;

    // Switch back and forth between stacks and contexts.
    // Both entry and exit point for the IRQ context that the storage task runs in.
    extern __attribute__((naked,noinline)) void contextSwitch() asm(SENSORTASK_VECTOR);
    void contextSwitch()
    {
        // refers to the taskStackPtr variable above (mangled C++ name)
        CONTEXTSWITCH("_ZN10SensorTask12taskStackPtrE");
    }

    // Wrapper for contextSwitch(), which ensures that hardware timer wakeup IRQs are cleared
    void yield()
    {
        contextSwitch();
        Timer::acknowledgeIRQ(&TICK_TIMER);
    }

    void init()
    {
        taskStack[ARRAYLEN(taskStack) - 1] = (uint32_t)run;  // Entry point
        Timer::enableIRQ(&TICK_TIMER, true);
        IRQ::wakeSensorTask();
    }
}
