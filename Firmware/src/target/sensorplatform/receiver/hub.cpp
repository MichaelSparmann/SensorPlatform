// SensorPlatform Base Station Data Hub
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

// This code takes care of processing USB commands and received packets


#include "global.h"
#include "hub.h"
#include "sys/time.h"
#include "usb.h"
#include "radio.h"


namespace Hub
{
    // Counts "RF packet received" notification USB messages
    static uint16_t pktIndex;

    void dpcHandlePackets()
    {
        USB::Packet* txBuf = USB::getPacketBuf();
        RF::Packet* rxPacket;
        
        // Handle received RF packet while there are any (and free USB buffers).
        // This has higher priority than command handling in order to avoid buffer overruns.
        // Back pressure can be put both on USB commands and radio communication,
        // but the latter should be avoided because of limited buffer space on the sensor nodes.
        while (txBuf && (rxPacket = Radio::getNextRxPacket()))
        {
            // Build and enqueue "RF packet received" notification USB packet
            memset(txBuf, 0, 32);
            txBuf->header.type = USB::NID_RFPacketReceived;
            txBuf->notify.rfPacketReceived.sofCount = Radio::stats.sofTotal;
            txBuf->notify.rfPacketReceived.rxCount = Radio::stats.rxAcked;
            txBuf->notify.rfPacketReceived.index = pktIndex++;
            memcpy(txBuf->notify.rfPacketReceived.packet, rxPacket, sizeof(*rxPacket));
            Radio::releaseRxPacket();
            USB::submitPacket();
            // Grab new USB buffer for the next iteration
            txBuf = USB::getPacketBuf();
        }
        
        // Process USB command packets while we still have USB buffer space left
        USB::Packet* cmd;
        while (txBuf && (cmd = USB::getPacket()))
        {
            // Prepare response packet
            memset(txBuf, 0, sizeof(*txBuf));
            txBuf->header.type = USB::RID_CommandResult;
            txBuf->header.seq = cmd->header.seq;
            // Every handler should set this. If one doesn't, the status will be HandlerFailed.
            txBuf->reply.commandResult.status = USB::Status_HandlerFailed;
            // This flag can be set to false if we are unable to handle a command immediately.
            // In that case, the handler will be called again later.
            bool handled = true;
            // This flag is set by handlers to indicate whether a response packet should be sent.
            // We will only send responses of the sequence number in the command was non-zero.
            bool tx = !!cmd->header.seq;
            switch (cmd->header.type)
            {
            case USB::CID_GetRadioStats:
                // Get radio interface statistics / telemetry
                memcpy(&txBuf->reply.commandResult.getRadioStats, &Radio::stats, sizeof(Radio::stats));
                txBuf->reply.commandResult.getRadioStats.localTime = read_usec_timer();
                txBuf->reply.commandResult.status = USB::Status_OK;
                break;

            case USB::CID_StopRadio:
                // Stop radio communication
                Radio::shutdown();
                txBuf->reply.commandResult.status = USB::Status_OK;
                break;

            case USB::CID_StartRadio:
                // Configure the radio interface as specified and start communication
                Radio::configure(&cmd->cmd.startRadio.channel);
                txBuf->reply.commandResult.status = USB::Status_OK;
                break;

            case USB::CID_PollDevice:
                // Enqueue time slots for polling a list of NodeIds.
                for (uint32_t i = 0; i < ARRAYLEN(cmd->cmd.assignSlots.slot); i++)
                    if (cmd->cmd.assignSlots.slot[i].owner)  // For all non-zero entries:
                    {
                        // Check if that NodeId is already enqueued to be polled
                        uint32_t slot;
                        for (slot = 0; slot < ARRAYLEN(Radio::nextPacketSlots); slot++)
                            if (Radio::nextPacketSlots[slot].owner == cmd->cmd.assignSlots.slot[i].owner) break;
                        if (slot >= ARRAYLEN(Radio::nextPacketSlots))  // If not:
                        {
                            // Attempt to find a free time slot during the next frame
                            // (Race conditions don't matter here because slots will
                            // never be allocated from a higher priority level)
                            for (slot = 0; slot < ARRAYLEN(Radio::nextPacketSlots); slot++)
                                if (!Radio::nextPacketSlots[slot].sticky && !Radio::nextPacketSlots[slot].owner) break;
                            if (slot >= ARRAYLEN(Radio::nextPacketSlots))  // If there are none:
                            {
                                handled = false;  // Retry later
                                break;
                            }
                            // If the node wasn't already enqueued and there is free slot, assign it
                            Radio::nextPacketSlots[slot].owner = cmd->cmd.assignSlots.slot[i].owner;
                        }
                        // Remove the node from the list in the USB packet,
                        // in case we can't honor all requests immediately and re-run this later.
                        cmd->cmd.assignSlots.slot[i].owner = 0;
                    }
                // If this line is reached, all requests from the packet have been honored.
                txBuf->reply.commandResult.status = USB::Status_OK;
                break;

            case USB::CID_AssignSlots:
                // Set permanent time slot assignments (0: Assign dynamically)
                memcpy(Radio::nextPacketSlots, cmd->cmd.assignSlots.slot, sizeof(Radio::nextPacketSlots));
                txBuf->reply.commandResult.status = USB::Status_OK;
                break;

            case USB::CID_TransmitCommand:
            {
                // Transmit a radio packet
                RF::Packet* packet = Radio::getCommandBuffer();
                if (!packet) handled = false;  // If there was no buffer space, retry later
                else
                {
                    // Enqueue the radio packet
                    memcpy(packet, cmd->cmd.transmitCommand.command, sizeof(*packet));
                    Radio::enqueueCommand(cmd->cmd.transmitCommand.targetNode);
                    txBuf->reply.commandResult.status = USB::Status_OK;
                }
                break;
            }

            default:
                txBuf->reply.commandResult.status = USB::Status_UnknownCommand;
                break;
            }
            
            // If we need to send a response, enqueue that and
            // grab a new buffer for the next iteration
            if (tx)
            {
                USB::submitPacket();
                txBuf = USB::getPacketBuf();
            }
            // If we have fully processed the USB command, discard it
            if (handled) USB::markProcessed();
        }
        
        // If the USB core had run out of packets to transmit,
        // and we have enqueued some new ones, restart transmission.
        USB::tryStartTx();
    }

}
