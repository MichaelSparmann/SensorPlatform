// Sensor node radio command handling
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
#include "commands.h"
#include "sys/time.h"
#include "sys/util.h"
#include "common.h"
#include "radio.h"
#include "storagetask.h"
#include "sensortask.h"


namespace Commands
{
    static bool measuring;  // Whether a measurement is currently in progress
    static bool uploadDirty;  // Whether the data upload buffer was modified since the last write

    bool handlePacket(RF::Packet::Command* cmd)
    {
        // Acquire a radio packet buffer for the response. If none is available,
        // handle the command later. Once a buffer is free we will be called again.
        RF::Packet::Reply* reply = Radio::getFreeTxBuffer(0);
        if (!reply) return false;
        
        // Initialize response buffer with message type, command sequence number and result.
        // Every code path should overwrite the initial result (HandlerFailed) with more specific
        // information. This is just a guard against bugs causing spurious success responses.
        memset(reply, 0, sizeof(*reply));
        reply->cmd.messageType = RF::RT_CommandResponse;
        reply->cmd.seq = cmd->header.seq;
        reply->cmd.result = RF::Result_HandlerFailed;
        
        // Whether we want to actually transmit a response (handlers may set this to false)
        bool tx = true;
        
        switch (cmd->header.cmd)
        {
        case RF::CID_ReadPageSeries:  // Read series header page (not sensor configuration)
            // Check if the requested page number is within bounds
            if (cmd->header.arg >= ARRAYLEN(mainBuf.seriesHeader.info.page))
                reply->cmd.result = RF::Result_InvalidArgument;
            // Check if the sensor and storage tasks are ready to accept the request
            else if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Fill response with the requested page contents and report success
                memcpy(reply->readPage.data, &mainBuf.seriesHeader.info.page[cmd->header.arg],
                       sizeof(reply->readPage.data));
                reply->cmd.result = RF::Result_OK;
            }
            // The sensor or storage task are busy and the main data buffer might not contain valid
            // series header information. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_WritePageSeries:  // Write series header page (not sensor configuration)
            // Check if the requested page number is within bounds
            if (cmd->header.arg >= ARRAYLEN(mainBuf.seriesHeader.info.page))
                reply->cmd.result = RF::Result_InvalidArgument;
            // Check if the sensor and storage tasks are ready to accept the request
            else if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Copy the received data into the requested series header page
                memcpy(&mainBuf.seriesHeader.info.page[cmd->header.arg],
                       cmd->writePage.data, sizeof(cmd->writePage.data));
                // Flag series header as modified
                StorageTask::seriesHeaderDirty = true;
                // Copy data back to response (we will never reject any series header change)
                memcpy(reply->readPage.data,
                       &mainBuf.seriesHeader.sensor[cmd->header.arg >> 2].page[cmd->header.arg & 3],
                       sizeof(reply->readPage.data));
                reply->cmd.result = RF::Result_OK;
            }
            // The sensor or storage task are busy and the main data buffer might not contain valid
            // series header information. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_ReadPageSensor:  // Read sensor configuration page
            // No bounds check needed here: The page field has 8 bits and there are 256 pages.
            // Check if the sensor and storage tasks are ready to accept the request.
            if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Fill response with the requested page contents and report success
                memcpy(reply->readPage.data,
                       &mainBuf.seriesHeader.sensor[cmd->header.arg >> 2].page[cmd->header.arg & 3],
                       sizeof(reply->readPage.data));
                reply->cmd.result = RF::Result_OK;
            }
            // The sensor or storage task are busy and the main data buffer might not contain valid
            // series header information. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_WritePageSensor:  // Write sensor configuration page
            // No bounds check needed here: The page field has 8 bits and there are 256 pages.
            // Check if the sensor and storage tasks are ready to accept the request.
            if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Attempt to apply the received configuration data to the sensor.
                // The sensor driver will clean it up if necessary and report success
                // if any change was applied successfully.
                reply->cmd.result = SensorTask::writeSensorPage(cmd->header.arg, cmd->writePage.data);
                // Copy back the actually applied configuration to the response.
                memcpy(reply->readPage.data,
                       &mainBuf.seriesHeader.sensor[cmd->header.arg >> 2].page[cmd->header.arg & 3],
                       sizeof(reply->readPage.data));
            }
            // The sensor or storage task are busy and the main data buffer might not contain valid
            // series header information. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_SaveConfig:  // Save node configuration to the SD card
            // Check if the sensor and storage tasks are ready to accept the request.
            if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Do as requested. Blocks until finished.
                StorageTask::saveConfig();
                reply->cmd.result = RF::Result_OK;
            }
            // The sensor or storage task is busy, so we cannot rely on the storage task being able
            // to handle the request. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_SaveSeriesHeader:  // Save series header data to the SD card
            // Check if the sensor and storage tasks are ready to accept the request.
            if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Do as requested. Blocks until finished.
                StorageTask::saveSeriesHeader();
                reply->cmd.result = RF::Result_OK;
            }
            // The sensor or storage task is busy, so we cannot rely on the storage task being able
            // to handle the request. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_StartMeasurement:  // Initiate a measurement
            // If we are already measuring, this is probably a retransmission due to a lost
            // response. Just retransmit the response (which was success) and ignore the request.
            if (measuring) reply->cmd.result = RF::Result_OK;
            // Check if the sensor and storage tasks are ready to accept the request.
            else if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
            {
                // Figure out the local microsecond time at which the measurement should begin.
                // The earliest possible time that a base station timestamp (28 bits) can refer to:
                int base = read_usec_timer() - (1 << 27);
                // That, plus (timestamp + delta) clamped to 28 bits, is the equivalent local time:
                int time = base + ((cmd->startMeasurement.atGlobalTime - Radio::globalTimeOffset - base) & 0xfffffff);
                // Write node hardware globally unique ID into the series header
                memcpy(&mainBuf.seriesHeader.info.hwId, &config.nodeUniqueId, sizeof(config.nodeUniqueId));
                // Write start time information into the series header:
                mainBuf.seriesHeader.info.localTime = time;
                mainBuf.seriesHeader.info.globalTime = cmd->startMeasurement.atGlobalTime;
                mainBuf.seriesHeader.info.unixTime = cmd->startMeasurement.unixTime;
                // Save the series header to the SD card configuration area
                StorageTask::saveSeriesHeader();
                // Initialize lost packet counters
                StorageTask::bufferOverflowLost = 0;
                Radio::bufferOverflowLost = 0;
                // Start up sensors (the series header is already in the buffer
                // and will be marked as ready to be written / sent by this)
                SensorTask::startMeasurement(time);
                // Start up data sinks (which will then write / transmit the series header)
                if (cmd->header.arg & 2) StorageTask::startRecording();
                if (cmd->header.arg & 1) Radio::startMeasurementTransmission();
                // Send success response
                measuring = true;
                reply->cmd.result = RF::Result_OK;
            }
            // The sensor or storage task is busy, so we cannot start a measurement right now.
            // Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_StopMeasurement:  // Stop the running measurement
            // If we are measuring, stop the measurement.
            if (measuring)
            {
                SensorTask::stopMeasurement();
                measuring = false;
            }
            // Report success whether we were measuring or not. If we weren't, this request is
            // either a retransmission due to a lost response or just an blind attempt to get
            // the node into a known state on initialization. In that case, just retransmit
            // the response (which was success) and ignore the request.
            reply->cmd.result = RF::Result_OK;
            reply->stopMeasurement.endTime = SensorTask::endTime;
            reply->stopMeasurement.endOffset = SensorTask::endOffset;
            reply->stopMeasurement.liveTxLost = Radio::bufferOverflowLost;
            reply->stopMeasurement.sdWriteLost = StorageTask::bufferOverflowLost;
            break;

        case RF::CID_StartUpload:  // Put storage task into upload mode
            // Check if the sensor and storage tasks are ready to accept the request.
            if (SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
                StorageTask::startUpload();
            // If we are in the desired target state (whether or not that is due to this command),
            // report success. This takes care of retransmissions due to lost responses.
            if (StorageTask::state == StorageTask::State_Uploading) reply->cmd.result = RF::Result_OK;
            // The sensor or storage task is busy, so we cannot rely on the storage task sector
            // buffer not being in use. Reject the request and tell the client to try again later.
            else reply->cmd.result = RF::Result_Busy;
            break;

        case RF::CID_StopUpload:  // Leave upload mode without triggering firmware upgrade
            // If we are in upload mode, leave it.
            if (StorageTask::state == StorageTask::State_Uploading) StorageTask::endUpload();
            // If we are in the desired target state (whether or not that is due to this command),
            // report success. This takes care of retransmissions due to lost responses.
            if (StorageTask::state == StorageTask::State_Idle) reply->cmd.result = RF::Result_OK;
            // If we were in any other state, this request makes no sense at all. Reject it.
            else reply->cmd.result = RF::Result_InvalidArgument;
            break;

        case RF::CID_UploadData:  // Upload a 28-byte data block to the storage sector buffer
            // This request is only allowed in upload mode.
            if (StorageTask::state == StorageTask::State_Uploading)
            {
                // Mark sector buffer as modified
                // (so that we can ignore retransmitted WriteSector requests if this flag is false)
                uploadDirty = true;
                // Copy uploaded data to the requested sector buffer page and report success
                StorageTask::uploadData(cmd->header.arg, cmd->writePage.data);
                reply->cmd.result = RF::Result_OK;
            }
            else reply->cmd.result = RF::Result_InvalidArgument;
            break;

        case RF::CID_WriteSector:  // Write sector buffer to the SD card
            // This request is only allowed in upload mode.
            if (StorageTask::state == StorageTask::State_Uploading)
            {
                // If the sector buffer was not modified since the last WriteSector request,
                // assume that this request is a retransmission and that we can ignore it.
                // TODO: We probably should compare the sector number to the previous request, too.
                if (uploadDirty)
                {
                    uploadDirty = false;
                    // Actually write the sector buffer to the requested SD card sector
                    StorageTask::writeSector(cmd->writeSector.sector);
                }
                // Report success, whether or not we ignored the command
                reply->cmd.result = RF::Result_OK;
            }
            else reply->cmd.result = RF::Result_InvalidArgument;
            break;

        case RF::CID_UpgradeFirmware:  // Trigger firmware update (using updater in sector buffer)
            // This request is only allowed in upload mode.
            if (StorageTask::state == StorageTask::State_Uploading)
                StorageTask::upgradeFirmware(cmd->upgradeFimware.size, cmd->upgradeFimware.crc);
            // If we are in the desired target state (whether or not that is due to this command),
            // report success. This takes care of retransmissions due to lost responses.
            if (StorageTask::state == StorageTask::State_Upgrading) reply->cmd.result = RF::Result_OK;
            // If we were in any other state, this request makes no sense at all. Reject it.
            else reply->cmd.result = RF::Result_InvalidArgument;
            break;

        case RF::CID_Reboot:  // Reboot the node immediately (without acknowledgement)
            reset();
            break;

        default: reply->cmd.result = RF::Result_UnknownCommand;
        }
        
        // If the command handler wants to transmit a response packet, do that
        if (tx) Radio::enqueuePacket(TX_ATTEMPTS_RESPONSE);
        
        // The command packet was consumed. Report that to the packet processing loop.
        return true;
    }
}
