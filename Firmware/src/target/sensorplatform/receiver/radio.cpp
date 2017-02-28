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

// See thesis paper for theory of operation.


#include "global.h"
#include "radio.h"
#include "soc/stm32/gpio.h"
#include "soc/stm32/exti.h"
#include "sys/util.h"
#include "sys/time.h"
#include "../common/driver/timer.h"
#include "driver/clock.h"
#include "driver/spi.h"
#include "driver/dma.h"
#include "irq.h"


#define RADIO_DMA_RX_REGS STM32_DMA_STREAM_REGS(RADIO_DMA_RX_CONTROLLER, RADIO_DMA_RX_STREAM)
#define RADIO_DMA_TX_REGS STM32_DMA_STREAM_REGS(RADIO_DMA_TX_CONTROLLER, RADIO_DMA_TX_STREAM)
#ifdef RADIO_SPI_ALWAYS_ON
#define RADIO_SPI_ON() {}
#define RADIO_SPI_OFF() {}
#else
#define RADIO_SPI_ON() Clock::onFromPri0(RADIO_SPI_CLK)
#define RADIO_SPI_OFF() Clock::offFromPri0(RADIO_SPI_CLK)
#endif


namespace Radio
{

    static const DMA::Config dmaRxCfg(RADIO_DMA_RX_CHANNEL, RADIO_DMA_RX_PRIORITY,
                                      DMA::DIR_P2M, DMA::TS_32BIT, true, DMA::TS_8BIT, false, true);
    static const DMA::Config dmaTxCfg(RADIO_DMA_TX_CHANNEL, RADIO_DMA_TX_PRIORITY,
                                      DMA::DIR_M2P, DMA::TS_32BIT, true, DMA::TS_8BIT, false, true);
    static const DMA::Config dummyTxCfg(RADIO_DMA_TX_CHANNEL, RADIO_DMA_TX_PRIORITY,
                                        DMA::DIR_M2P, DMA::TS_32BIT, false, DMA::TS_8BIT, false, false);
    static const NRF::NRF24L01P::DataRate dataRateMapping[] =
    {
        NRF::NRF24L01P::DataRate_2Mbit,
        NRF::NRF24L01P::DataRate_1Mbit,
        NRF::NRF24L01P::DataRate_1Mbit,  // Invalid, reserved
        NRF::NRF24L01P::DataRate_250Kbit,
    };
    static int32_t dummyData = -1;


    // Whether the radio was configured since the last shutdown (and is thus currently operating)
    static bool operating;

    // Beacon packet that advertizes the channel controlled by this device
    static RF::Packet::Beacon beaconPacket;

    // Buffer that is used to build SOF packets, also keeps track of the sequence number
    static RF::Packet::SOF sofPacket;

    // The full timestamp from the current SOF packet
    static int sofTimestamp;

    // Planned slot owners for the next frame
    RF::TimeSlotOwner nextPacketSlots[];

    // Radio statistics
    USB::RadioStats stats;

    // The time at which the transmission of the last SOF packet was completed
    static int frameStartTime;

    // Current radio configuration, switches between PTX and PRX mode
    static NRF::NRF24L01P::Config radioCfg(NRF::Radio::Role_PTX, true, NRF::Radio::CrcMode_16Bit, true, false, false);

    // Current radio RF setup, cached here so that we can switch back to the
    // settings of our own channel easily after sending a beacon packet.
    static NRF::NRF24L01P::RfSetup rfSetup;

    enum State
    {
        State_WaitForRx = 0,  // Waiting for data packets to arrive, blocked on radio IRQ or timer
        State_DownloadRx,  // Downloading received packet, blocked on RX DMA
        State_DownloadBeforePTX,  // We were downloading a packet when the timer hit, switch to PTX once that finishes
        State_DownloadBeforeSOF,  // Downloading received packet while locking PLL for SOF TX, blocked on RX DMA
        State_UploadSOF,  // Uploading SOF packet, blocked on TX DMA
        State_WaitSentBeforeSOF,  // Transmitting dummy packet, blocked on radio IRQ, the next packet is SOF
        State_UploadBeacon,  // Uploading beacon packet, blocked on TX DMA
        State_WaitSentBeforeBeacon,  // Transmitting SOF packet, blocked on radio IRQ, this will be a beacon frame
        State_UploadCommand,  // Uploading command packet, blocked on TX DMA
        State_WaitSentBeforeCommand,  // Transmitting something packet, blocked on radio IRQ, there will more commands
        State_WaitSentBeforePRX,  // Transmitting last packet, blocked on radio IRQ, will switch to PRX mode after that
    } currentState;

    // Whether a DMA transfer is currently controlling the SPI bus (locking out IRQs)
    bool dmaActive;

    // This counts the number of consecutive frames that have been used to send commands, including the current one.
    // If it is nonzero, the current frame contains command slots instead of a beacon. It is also used to ensure
    // that beacons are sent periodically even if command packets are available during every frame by enforcing a limit.
    static uint8_t cmdFrameCount;

    // The time at which we received the packet that's currently being downloaded via DMA
    static int lastRxTime;
    // The slot number that we associated the previously received packet with.
    static int8_t prevRxSlot;

    // How many command packets we have already uploaded during this frame
    static uint8_t commandsSent;

    // Ring buffer of pending commands and their target addresses
    static uint8_t cmdTarget[RADIO_CMD_BUFFERS];
    static RF::Packet cmdData[ARRAYLEN(cmdTarget)];
    static uint8_t cmdWritePtr;
    static uint8_t cmdReadPtr;

    // Ring buffer of received packets
    static RF::Packet rxPacket[RADIO_RX_BUFFERS];
    static uint8_t rxWritePtr;
    static uint8_t rxReadPtr;

    // Slot to node assignment tracking info.
    enum NodePriority
    {
        Priority_Disconnected = 0,
        Priority_NoDataSkip,
        Priority_SingleSlotPoll,
        Priority_Urgency0,
        Priority_Urgency1,
        Priority_Urgency2,
        Priority_Urgency3,
        Priority_Urgency4,
        Priority_Urgency5,
        Priority_Urgency6,
        Priority_Urgency7,
        PRIORITY_COUNT,
        PRIORITY_MAX = PRIORITY_COUNT - 1
    };
    static struct NodeInfo
    {
        uint8_t prev;
        uint8_t next;
        NodePriority priority : 4;
        uint8_t : 4;
        uint8_t frames;  // Number of frames to be skipped if priority == 1, number of lost frames otherwise
        RF::Packet::Reply::Header::BufferInfo info;
    } nodeInfo[RF::MaxNode + 1];  // Array element 0 should never be accessed
    static uint8_t priorityCount[PRIORITY_COUNT];  // Number of elements in each priority level's linked list
    static uint8_t priorityHead[PRIORITY_COUNT];  // Linked list head node IDs for each priority level
    static uint8_t priorityTail[PRIORITY_COUNT];  // Linked list tail node IDs for each priority level


    // Move a node to a different priority list
    static void setNodePriority(int nodeId, NodePriority newPriority)
    {
        // Calculate list entry address
        NodeInfo* node = nodeInfo + nodeId;
        int oldPriority = node->priority;
        // If the priority didn't change, we don't need to do anything.
        if (oldPriority == newPriority) return;
        node->priority = newPriority;
        // Update list element counts. (This whole function can't be preempted, don't worry about races here.)
        priorityCount[oldPriority]--;
        priorityCount[newPriority]++;
        // Unlink the node from its current list
        if (node->next) nodeInfo[node->next].prev = node->prev;
        else priorityTail[oldPriority] = node->prev;
        if (node->prev) nodeInfo[node->prev].next = node->next;
        else priorityHead[oldPriority] = node->next;
        // Insert it as the head of its target list
        node->prev = 0;
        node->next = priorityHead[newPriority];
        priorityHead[newPriority] = nodeId;
        if (node->next) nodeInfo[node->next].prev = nodeId;
        else priorityTail[newPriority] = nodeId;
    }


    static void setState(State newState)
    {
        currentState = newState;
    }


    // Send a command to the radio chip
    static NRF::SPI::Status sendCmd(uint8_t cmd)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        NRF::SPI::Status status(SPI::xferByte(&RADIO_SPI_BUS, cmd));
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
        return status;
    }


    // Query radio chip status
    static NRF::SPI::Status getStatus()
    {
        return sendCmd(NRF::SPI::Cmd_GetStatus);
    }


    // Write radio chip configuration register
    static NRF::SPI::Status writeReg(uint8_t reg, uint8_t data)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, NRF::SPI::Cmd_WriteReg | reg);
        NRF::SPI::Status status(SPI::pullByte(&RADIO_SPI_BUS));
        SPI::pushByte(&RADIO_SPI_BUS, data);
        SPI::pullByte(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
        return status;
    }

    
    // Initiate DMA radio packet upload
    static void startPacketUpload(void* packet, size_t len)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, NRF::SPI::Cmd_WritePacket & 0xff);
        DMA::startTransferFromPri0(&RADIO_DMA_TX_REGS, dmaTxCfg, packet, len);
        dmaActive = true;
        GPIO::setLevelFast(PIN_LED3, true);
        // Disable the radio IRQ, to prevent it from interfering with the SPI bus.
        IRQ::enableRadioIRQ(false);
    }


    // Initiate DMA download of a received radio packet
    static void startPacketDownload(void* packet, size_t len)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, NRF::SPI::Cmd_ReadPacket);
        // Get the status byte out of the way, we don't want it to end up in the DMA receive buffer.
        SPI::pullByte(&RADIO_SPI_BUS);
        DMA::startTransferFromPri0(&RADIO_DMA_RX_REGS, dmaRxCfg, packet, len);
        DMA::startTransferFromPri0(&RADIO_DMA_TX_REGS, dummyTxCfg, &dummyData, len);
        dmaActive = true;
        GPIO::setLevelFast(PIN_LED3, true);
        // Disable the radio IRQ, to prevent it from interfering with the SPI bus.
        IRQ::enableRadioIRQ(false);
    }


int slack, space, postproctime;  // Debug instrumentation
    // Start uploading the SOF packet. Expects SPI core to be powered up.
    static void sendSOF()
    {
        // Copy fixed slot assignments, count how many slots are free, and mark those as notification slots for now.
        int freeSlots = 0;
        for (uint32_t i = 0; i < ARRAYLEN(sofPacket.slot); i++)
        {
            uint8_t owner = nextPacketSlots[i].owner;
            if (!owner)
            {
                owner = RF::Address::Notify;
                freeSlots++;
            }
            sofPacket.slot[i].owner = owner;
        }
        // Figure out when we should hurry up if the SOF packet isn't finished yet.
        int timeout = sofTimestamp + 75;
space = timeout - read_usec_timer();  // Debug instrumentation
        // While we have time to do so (~50µs), try to assign slots based on
        // buffer level information reported back by nodes during the last frame.
        int slot = 0;
        for (uint32_t priority = PRIORITY_MAX;
             !TIMEOUT_EXPIRED(timeout) && freeSlots && priority >= Priority_SingleSlotPoll; priority--)
        {
            // Figure out how many slots we may assign to this whole priority level,
            // and how many nodes are competing for those.
            int slots = MIN(freeSlots, MAX_SLOTS_PER_PRIORITY);
            int count = priorityCount[priority];
            if (!count) continue;
            // Calculate how many slots we can assign per node.
            // This ignores how many packets these nodes will actually be able to send,
            // and may thus pass on a few extra slots to lower priority levels, which might be a good idea anyway.
            count = MIN(slots, count);
            if (priority == Priority_SingleSlotPoll) slots = 1;
            else slots /= count;
            // Try to assign the calculated number of slots to the first nodes at this priority level.
            int nodeId = priorityHead[priority];
            NodeInfo* node = nodeInfo + nodeId;
            while (!TIMEOUT_EXPIRED(timeout) && count--)
            {
                // Figure out how many slots we can (sensibly) assign to that node.
                int assign = node->info.pendingPackets - 1;
                assign = MAX(assign, 1);
                assign = MIN(assign, slots);
                assign = MIN(assign, MAX_SLOTS_PER_NODE);
                // Assign that number of slots.
                freeSlots -= assign;
                while (assign--)
                {
                    // Find the next free slot.
                    while (sofPacket.slot[slot].owner != RF::Address::Notify) slot++;
                    // Assign the slot to the current node.
                    sofPacket.slot[slot].owner = nodeId;
                }
                // Increment frame loss counter of that node.
                // (Will be reset to 0 if we receive any packets from the node during the frame.)
                node->frames++;
                // Move on to the next node.
                nodeId = node->next;
                node = nodeInfo + nodeId;
            }
            // Move the nodes that we have processed to the tail of the list.
            if (nodeId)
            {
                int oldTail = priorityTail[priority];
                int oldHead = priorityHead[priority];
                nodeInfo[oldTail].next = oldHead;
                nodeInfo[oldHead].prev = oldTail;
                int newTail = node->prev;
                nodeInfo[newTail].next = 0;
                node->prev = 0;
                priorityHead[priority] = nodeId;
                priorityTail[priority] = newTail;
            }
        }
slack = timeout - read_usec_timer();  // Debug instrumentation
        // Upload the completed SOF packet
        startPacketUpload(&sofPacket, sizeof(sofPacket));
        // While we're waiting for DMA to finish, make use of the time to clean out fixed slot assignments
        // decrement frame skip counters and move nodes that have lost too many frames to single-slot polling.
        // We should do that before handing off control to a lower priority level.
int postprocstart = read_usec_timer();  // Debug instrumentation
        for (uint32_t i = 0; i < ARRAYLEN(nextPacketSlots); i++)
            if (!nextPacketSlots[i].sticky)
                nextPacketSlots[i].owner = 0;
        for (int nodeId = priorityHead[Priority_NoDataSkip]; nodeId; nodeId = nodeInfo[nodeId].next)
            if (!--nodeInfo[nodeId].frames)
                setNodePriority(nodeId, Priority_SingleSlotPoll);
        for (uint32_t i = 0; i < ARRAYLEN(sofPacket.slot); i++)
        {
            uint32_t nodeId = sofPacket.slot[i].owner;
            if (!nodeId || nodeId >= ARRAYLEN(nodeInfo)) continue;
            NodeInfo* node = nodeInfo + nodeId;
            if (node->priority == Priority_SingleSlotPoll)
            {
                if (node->frames > NODE_FRAME_LOSS_DISCONNECT)
                    setNodePriority(nodeId, Priority_Disconnected);
            }
            else if (node->frames > NODE_FRAME_LOSS_SINGLESLOT)
            {
                node->frames = 0;
                setNodePriority(nodeId, Priority_SingleSlotPoll);
            }
        }
        stats.sofTotal++;
postproctime = read_usec_timer() - postprocstart;  // Debug instrumentation
        // The DMA completion IRQ handler will take care of the rest. Do not shut down the SPI bus.
        setState(State_UploadSOF);
        GPIO::setLevelFast(PIN_LED2, false);
    }


    // Check if there are pending RX packets and start downloading one if yes. Expects SPI core to be powered up.
    static bool checkReceived(State downloadState, int time)
    {
        // Acknowledge the IRQ and figure out which pipe we received the packet on, or if there even is one.
        NRF::SPI::Status status = writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(false, false, true).d8);
        // Check if there was something in the RX FIFO
        if (status.b.rxPipe < 6)
        {
            // Do we have space in our receive buffer?
            int free = rxReadPtr - rxWritePtr - 1;
            if (free < 0) free += ARRAYLEN(rxPacket);
            if (free)
            {
                lastRxTime = time;
                startPacketDownload(rxPacket + rxWritePtr, sizeof(*rxPacket));
                // The DMA completion IRQ handler will take care of the rest and call us again once it's finished.
                setState(downloadState);
                return true;
            }
            else
            {
                // No space in the buffer, we can't do much about that.
                sendCmd(NRF::SPI::Cmd_FlushRx);
                stats.rxOverflow++;
                GPIO::setLevelFast(PIN_LED2, true);
            }
        }
        // If we are in PTX mode already, the final SOF packet can be uploaded now
        if (downloadState == State_DownloadBeforeSOF)
        {
            sendSOF();
            return true;
        }
        // Otherwise go back to idle state, waiting for further RX packets or a frame timer tick.
        setState(State_WaitForRx);
        return false;
    }


    // Upload a 1 byte dummy packet to the radio and set the transmission address to the dummy value.
    static void prepareNextDummyPacket()
    {
        // Set the transmission address to the dummy channel and also fix the netId, we might have sent a beacon before.
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_TxAddress);
        SPI::pushByte(&RADIO_SPI_BUS, RF::Address::Dummy);
        SPI::pushByte(&RADIO_SPI_BUS, beaconPacket.channelAttrs.netId);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
        // Push a 1 byte dummy packet into the TX FIFO.
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WritePacket & 0xff));
        SPI::pushByte(&RADIO_SPI_BUS, 0);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
    }


    // Switch to PTX mode and start sending a dummy packet. Expects SPI core to be powered up.
    static void switchToPTX()
    {
        // Switch to PTX mode, a 1 byte dummy packet is already in the TX FIFO.
        // This is used to get the PLL to start up as early as possible, while we're still preparing the SOF packet.
        GPIO::setLevelFast(PIN_RADIO_CE, false);
        radioCfg.b.role = NRF::Radio::Role_PTX;
        writeReg(NRF::Radio::Reg_Config, radioCfg.d8);
        GPIO::setLevelFast(PIN_RADIO_CE, true);
        // Capture the time here, the rising edge of CE is effectively the sync point for the other nodes.
        sofTimestamp = read_usec_timer();
        sofPacket.info.time = sofTimestamp;
        sofPacket.info.seq++;
        // Sync the next frame to this point in time, to avoid early SOF situations if the SOF before was late.
        Timer::reset(&RADIO_TIMER);
        IRQ::clearRadioTimerIRQ();

        // Check if another packet arrived during the transition to TX mode.
        // This will start sending the SOF packet once all RX packets have been taken care of.
        checkReceived(State_DownloadBeforeSOF, sofTimestamp);
    }


    static bool trySendCommand()
    {
        // Check if we may send another command
        if (++commandsSent > beaconPacket.channelAttrs.cmdSlots) return false;
        // Check if there are commands in the buffer
        int used = cmdWritePtr - cmdReadPtr;
        if (used < 0) used += ARRAYLEN(cmdTarget);
        if (!used) return false;
        // We want to send commands, and actually have one ore more in the buffer, so let's upload one.
        startPacketUpload(cmdData + cmdReadPtr, sizeof(*cmdData));
        setState(State_UploadCommand);
        stats.txTotal++;
        return true;
    }


    // Frame timer tick handler
    void timerTick()
    {
        // It's time to initiate preparation and transmission of the next SOF packet.
        Timer::acknowledgeIRQ(&RADIO_TIMER);
        if (currentState == State_DownloadRx)
        {
            // DMA in progress. We need to wait for that to finish,
            // and tell the handler to call switchToPTX when done.
            setState(State_DownloadBeforePTX);
            return;
        }
        else if (currentState == State_WaitSentBeforePRX)
        {
            // Either this is the first frame, or we lost a packet or a TX_DS IRQ. We need to prepare for
            // sending the next frame, which would otherwise have been done by the WaitSentBeforePRX handler.
            RADIO_SPI_ON();
            prepareNextDummyPacket();
        }
        else if (currentState != State_WaitForRx) hang();
        RADIO_SPI_ON();
        switchToPTX();
    }


    void handleRXDMACompletion()
    {
        // RX packet download DMA completed, so figure out what we actually received here.
        GPIO::setLevelFast(PIN_RADIO_NCS, true);

        // Get rid of the accompanying dummy TX DMA transfer
        DMA::cancelTransferFromPri0(&RADIO_DMA_TX_REGS, RADIO_DMA_TX_CONTROLLER, RADIO_DMA_TX_STREAM);
        dmaActive = false;
        GPIO::setLevelFast(PIN_LED3, false);

        if (!operating) return;  // The radio is being shut down

        // Re-enable the radio IRQ, it won't preempt us and we won't be interfering with it after this handler finishes.
        // (Unless we start another DMA transfer, which would disable it again.)
        IRQ::enableRadioIRQ(true);

        // Figure out which slot this packet belongs to timing-wise:
        // Slot: 8 bits preamble, 24 bits address, 256 bits payload, 16 bits CRC, guard bits
        int slotBits = 8 + 24 + 256 + 16 + beaconPacket.channelAttrs.guardBits;
        int slotTime = (slotBits << beaconPacket.channelAttrs.speed) >> 1;
        int offsetTime = (beaconPacket.channelAttrs.offsetBits << beaconPacket.channelAttrs.speed) >> 1;
        int timeWithinFrame = lastRxTime - frameStartTime - offsetTime;
        int slot = (timeWithinFrame - slotTime / 16) / slotTime;
        if (slot <= prevRxSlot) slot = prevRxSlot + 1;
        if (slot < 0 || slot > 27) slot = 27;
        prevRxSlot = slot;

        // Check for acknowledgment conditions:
        uint8_t slotOwner = sofPacket.slot[slot].owner;
        uint8_t nodeId = rxPacket[rxWritePtr].reply.header.nodeId;
        if (slotOwner == nodeId && nodeId)
        {
            stats.rxAcked++;
            sofPacket.slot[slot].ack = true;

            if (nodeId < ARRAYLEN(nodeInfo))
            {
                // Update slot assignment priority info
                nodeInfo[nodeId].info = rxPacket[rxWritePtr].reply.header.bufferInfo;
                NodePriority priority = (NodePriority)(Priority_Urgency0 + nodeInfo[nodeId].info.urgency);
                if (rxPacket[rxWritePtr].reply.noData.messageId == RF::RID_NoData)
                {
                    // We have polled a node that has no data. Lock it out for the next frames if it requests that.
                    nodeInfo[nodeId].frames = rxPacket[rxWritePtr].reply.noData.pollInFrames;
                    if (nodeInfo[nodeId].frames) priority = Priority_NoDataSkip;
                    else priority = Priority_SingleSlotPoll;
                }
                else nodeInfo[nodeId].frames = 0;
                // Update linked lists
                setNodePriority(nodeId, priority);
            }
        }
        else stats.rxSlotNotOwned++;

        // Insert the packet into the buffer
        if (rxWritePtr + 1 == ARRAYLEN(rxPacket)) rxWritePtr = 0;
        else rxWritePtr++;
        IRQ::setPending(IRQ::DPC_HubHandlePackets);

        // If a frame timer tick arrived, take care of that ASAP. We'll call checkReceived from there.
        if (currentState == State_DownloadBeforePTX) switchToPTX();
        // Otherwise check for further RX packets.
        else if (!checkReceived(currentState, read_usec_timer()))
        {
            // Nothing left, we can shut down the SPI bus,
            RADIO_SPI_OFF();
        }
    }


    void handleTXDMACompletion()
    {
        // TX packet upload DMA completed, so mark the end of the packet by deselecting the device.
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
        dmaActive = false;
        GPIO::setLevelFast(PIN_LED3, false);

        if (!operating) return;  // The radio is being shut down

        // Re-enable the radio IRQ, it won't preempt us and we won't be interfering with it after this handler finishes.
        // (Unless we start another DMA transfer, which would disable it again.)
        IRQ::enableRadioIRQ(true);

        switch (currentState)
        {
        case State_UploadSOF:
            // We need to wait for the dummy packet to be sent.
            setState(State_WaitSentBeforeSOF);
            break;

        case State_UploadBeacon:
            // Deassert CE so that we don't send the beacon packet before switching to the discovery frequency.
            GPIO::setLevelFast(PIN_RADIO_CE, false);
            // Set the destination address for the beacon packet that we just uploaded.
            GPIO::setLevelFast(PIN_RADIO_NCS, false);
            SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_TxAddress);
            SPI::pushByte(&RADIO_SPI_BUS, RF::Address::Beacon);
            SPI::pushByte(&RADIO_SPI_BUS, RF::BEACON_NET_ID);
            SPI::waitDone(&RADIO_SPI_BUS);
            GPIO::setLevelFast(PIN_RADIO_NCS, true);
            // We need to wait for the SOF packet to be sent.
            setState(State_WaitSentBeforeBeacon);
            break;

        case State_UploadCommand:
            // Set the destination address for the command packet that we just uploaded, and remove it from the buffer.
            writeReg(NRF::Radio::Reg_TxAddress, cmdTarget[cmdReadPtr]);
            if (cmdReadPtr + 1 == ARRAYLEN(cmdTarget)) cmdReadPtr = 0;
            else cmdReadPtr++;
            IRQ::setPending(IRQ::DPC_HubHandlePackets);
            // We need to wait for the SOF or command packet to be sent.
            setState(State_WaitSentBeforeCommand);
            break;

        default: hang();
        }

        // We will definitely be waiting for transfer completion here, so we don't need the SPI bus anymore.
        RADIO_SPI_OFF();
    }


    // Radio chip IRQ handler
    void handleIRQ()
    {
        do
        {
            int now = read_usec_timer();
            RADIO_SPI_ON();
            NRF::SPI::Status status = getStatus();

            if (status.b.dataSent)
            {
                switch (currentState)
                {
                case State_WaitSentBeforeSOF:
                {
                    // The radio is currently transmitting the preamble of the SOF packet,
                    // we need to set the TX address for that immediately.
                    writeReg(NRF::Radio::Reg_TxAddress, RF::Address::SOF);
                    // Acknowledge the IRQ
                    writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(false, true, false).d8);
                    // Check if we want to send a command, and if yes, try to do so.
                    commandsSent = 0;
                    if (cmdFrameCount++ >= MAX_CONSECUTIVE_CMD_FRAMES || !trySendCommand())
                    {
                        // We either need to send a beacon, or there are no commands to be sent.
                        cmdFrameCount = 0;
                        // Start uploading the packet here, but pull CE low once that finishes,
                        // so that it will only be sent after switching to the beacon channel.
                        beaconPacket.seq++;
                        beaconPacket.time = read_usec_timer();
                        startPacketUpload(&beaconPacket, sizeof(beaconPacket));
                        // The DMA completion IRQ handler will take care of the rest
                        setState(State_UploadBeacon);
                    }
                    // Clean up old ack bits from the previous frame
                    for (uint32_t i = 0; i < ARRAYLEN(sofPacket.slot); i++) sofPacket.slot[i].ack = false;
                    break;
                }

                case State_WaitSentBeforeBeacon:
                    // Acknowledge the IRQ, for some reason this MUST happen before asserting CE (HW bug?)
                    writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(false, true, false).d8);
                    // We have finished transmitting the SOF packet. This is a reference point for slot alignment.
                    frameStartTime = now;
                    // Switch frequency and other settings to send a beacon.
                    writeReg(NRF::NRF24L01P::Reg_RfChannel, RF::BEACON_CHANNEL);
                    writeReg(NRF::NRF24L01P::Reg_RfSetup, RF::BEACON_RF_SETUP.d8);
                    // Start sending the beacon packet
                    GPIO::setLevelFast(PIN_RADIO_CE, true);
                    // The DMA completion IRQ handler will take care of the rest
                    setState(State_WaitSentBeforePRX);
                    break;

                case State_WaitSentBeforeCommand:
                    // If this is the first command, then we have just finished transmitting the SOF packet.
                    // This is a reference point for slot alignment.
                    if (commandsSent == 1) frameStartTime = now;
                    // Acknowledge the IRQ
                    writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(false, true, false).d8);
                    // Check if we want (and can) send another command. If yes, let's do that.
                    if (trySendCommand()) break;
                    // We don't have any commands left, so we can't make any use of the remaining command slots.
                    setState(State_WaitSentBeforePRX);
                    break;

                case State_WaitSentBeforePRX:
                    // Acknowledge the IRQ
                    writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(false, true, false).d8);
                    // We have finished the last TX packet of this frame, so we'll switch to PRX mode now.
                    GPIO::setLevelFast(PIN_RADIO_CE, false);
                    if (!cmdFrameCount)
                    {
                        // We have sent a beacon, so we need to switch back to our own frequency first.
                        writeReg(NRF::NRF24L01P::Reg_RfChannel, beaconPacket.channelAttrs.channel);
                        writeReg(NRF::NRF24L01P::Reg_RfSetup, rfSetup.d8);
                    }
                    radioCfg.b.role = NRF::Radio::Role_PRX;
                    writeReg(NRF::Radio::Reg_Config, radioCfg.d8);
                    GPIO::setLevelFast(PIN_RADIO_CE, true);
                    prevRxSlot = -1;
                    // Already prepare the TX pipe for the next dummy packet (before the next SOF packet)
                    prepareNextDummyPacket();
                    // We are ready to receive data packets from other nodes
                    setState(State_WaitForRx);
                    break;

                default: hang();
                }
            }

            if (status.b.dataReceived)
                switch (currentState)
                {
                case State_WaitForRx:
                    // A packet arrived, so let's download it.
                    if (checkReceived(State_DownloadRx, now)) return;
                    break;

                default: hang();
                }

            STM32::EXTI::clearPending(PIN_RADIO_NIRQ);
        } while (!GPIO::getLevelFast(PIN_RADIO_NIRQ));

        // Check if we still need the SPI bus
        if (!dmaActive) RADIO_SPI_OFF();
    }


    ////// Everything above this line is running in IRQ context (priority 0) /////
    //////////////////////////////////////////////////////////////////////////////
    ////// Everything below this line is running in DPC context (priority 3) /////


    // One-time radio hardware initialization
    void init()
    {
        // Set up the DMA controller for radio SPI bus access
        DMA::setPeripheralAddr(&RADIO_DMA_RX_REGS, SPI::getDataRegPtr(&RADIO_SPI_BUS));
        DMA::setPeripheralAddr(&RADIO_DMA_TX_REGS, SPI::getDataRegPtr(&RADIO_SPI_BUS));
        DMA::setFIFOConfig(&RADIO_DMA_RX_REGS, false, 0);
        DMA::setFIFOConfig(&RADIO_DMA_TX_REGS, false, 2);
        IRQ::enableRadioDMAIRQs(true);

        // Configure the SPI bus
        RADIO_SPI_ON();
        SPI::init(&RADIO_SPI_BUS);
        SPI::setFrequency(&RADIO_SPI_BUS, RADIO_SPI_PRESCALER);
        RADIO_SPI_OFF();
    }


    // Stop radio operation
    void shutdown()
    {
        if (!operating) return;
        operating = false;

        // Disable IRQs to prevent any race conditions here
        Timer::stop(&RADIO_TIMER, RADIO_TIMER_CLK);
        IRQ::enableRadioTimerIRQ(false);
        IRQ::enableRadioIRQ(false);

        // Wait for any outstanding DMA transfers to finish
        while (dmaActive);

        // Shut down the radio
        GPIO::setLevelFast(PIN_RADIO_CE, false);
        RADIO_SPI_ON();
        writeReg(NRF::Radio::Reg_Config, 0);  // Power down
        RADIO_SPI_OFF();
    }


    // Configure radio settings and start operation
    void configure(RF::ExtendedChannelAttributes* channelAttrs)
    {
        // Ensure that everything is stopped and that we're safe to modify the settings
        shutdown();

        // Copy radio channel attributes and calculate resulting setup and timings
        // Slot: 8 bits preamble, 24 bits address, 256 bits payload, 16 bits CRC
        int slotBits = 8 + 24 + 256 + 16;
        // PLL lock takes 130usec, translate that into bits.
        int pllBits = 264 >> channelAttrs->ca.speed;
        // Dummy packet: 8 bits preamble, 24 bits address, 8 bits payload, 16 bits CRC
        int dummyBits = 8 + 24 + 8 + 16;
        // SOF: PLL lock + dummy packet + SOF slot (from end of RX to end of SOF packet)
        int sofBits = pllBits + dummyBits + slotBits;
        // Beacon: Config change + PLL lock + 8 bits preamble, 24 bits address, 128 bits payload, 16 bits CRC
        // The data bits are sent at 1Mbit/s max. Translate those into bit times at our own channel speed.
        int beaconBits = 10 + pllBits + ((8 + 24 + 128 + 16) << (channelAttrs->ca.speed == 0 ? 1 : 0));
        // 2 command slots in 2Mbit/s mode, 1 command slot in 1Mbit/s or 250kbit/s mode
        channelAttrs->ca.cmdSlots = 1 + (channelAttrs->ca.speed == 0);
        // Offset from the end of the SOF packet to the beginning of the first RX slot
        channelAttrs->ca.offsetBits += MAX(beaconBits, channelAttrs->ca.cmdSlots * slotBits)
                                     + pllBits + channelAttrs->ca.guardBits + 94;
        // Total frame bits: SOF + offset + 28 slots * (slot bits + guard bits) + frame slack - timer-to-PLL offset
        int frameBits = sofBits + channelAttrs->ca.offsetBits + channelAttrs->tailBits
                      + ARRAYLEN(sofPacket.slot) * (slotBits + channelAttrs->ca.guardBits) + 0;
        memcpy(&beaconPacket.channelAttrs, &channelAttrs->ca, sizeof(beaconPacket.channelAttrs));
        rfSetup = NRF::NRF24L01P::RfSetup(true, (NRF::NRF24L01P::Power)channelAttrs->receiverTxPower,
                                          dataRateMapping[channelAttrs->ca.speed], false);

        // Shut down the radio chip and configure it
        RADIO_SPI_ON();
        // Power down
        writeReg(NRF::Radio::Reg_Config, 0);
        // Flush FIFOs
        sendCmd(NRF::SPI::Cmd_FlushTx);
        NRF::SPI::Status status = sendCmd(NRF::SPI::Cmd_FlushRx);
        // Clear all IRQs
        writeReg(NRF::Radio::Reg_Status, status.d8);
        // Get rid of Enhanced ShockBurst
        writeReg(NRF::NRF24L01P::Reg_FeatureCtl, NRF::NRF24L01P::FeatureCtl(false, false, false).d8);
        writeReg(NRF::NRF24L01P::Reg_AutoAckCtl, NRF::NRF24L01P::AutoAckCtl(false, false, false, false, false, false).d8);
        writeReg(NRF::NRF24L01P::Reg_DynLengthCtl, NRF::NRF24L01P::DynLengthCtl(false, false, false, false, false, false).d8);
        writeReg(NRF::NRF24L01P::Reg_RetransCtl, NRF::NRF24L01P::RetransCtl(0, 0).d8);
        // Disable all RX pipes for now
        writeReg(NRF::NRF24L01P::Reg_RxPipeEnable, NRF::NRF24L01P::RxPipeEnable(true, false, false, false, false, false).d8);
        // Configure address width, channel frequency, modulation etc.
        writeReg(NRF::NRF24L01P::Reg_AddressCtl, NRF::NRF24L01P::AddressCtl(NRF::NRF24L01P::Width_24Bit).d8);
        writeReg(NRF::NRF24L01P::Reg_RfChannel, channelAttrs->ca.channel);
        writeReg(NRF::NRF24L01P::Reg_RfSetup, rfSetup.d8);
        // We start up in PTX mode (does nothing before writing to the FIFO)
        radioCfg.b.role = NRF::Radio::Role_PTX;
        writeReg(NRF::Radio::Reg_Config, radioCfg.d8);

        // We will only ever receive 32 byte packets
        for (int i = 0; i < 6; i++) writeReg(NRF::Radio::Reg_RxDataLength0 + i, 32);

        // Configure TX pipe for first packet (dummy, address 00)
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_TxAddress);
        SPI::pushByte(&RADIO_SPI_BUS, RF::Address::Reply);
        SPI::pushByte(&RADIO_SPI_BUS, channelAttrs->ca.netId);
        SPI::pushByte(&RADIO_SPI_BUS, RF::PROTOCOL_ID);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);

        // Configure RX pipe 0 for response and notify messages (address 00)
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_RxPipeAddress0);
        SPI::pushByte(&RADIO_SPI_BUS, RF::Address::Reply);
        SPI::pushByte(&RADIO_SPI_BUS, channelAttrs->ca.netId);
        SPI::pushByte(&RADIO_SPI_BUS, RF::PROTOCOL_ID);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);

        // Enable the radio with the new configuration
        GPIO::setLevelFast(PIN_RADIO_CE, true);

        // Reset some internal state. Our current situation is very similar to WaitSentBeforePRX,
        // but we will not actually go to PRX mode. The timer IRQ handler will catch this.
        cmdFrameCount = MAX_CONSECUTIVE_CMD_FRAMES;
        setState(State_WaitSentBeforePRX);
        operating = true;
        memset(nextPacketSlots, 0, sizeof(nextPacketSlots));
        memset(&stats, 0, sizeof(stats));
        // Clean out slot assignment linked lists
        memset(nodeInfo, 0, sizeof(nodeInfo));
        memset(priorityCount, 0, sizeof(priorityCount));
        memset(priorityHead, 0, sizeof(priorityHead));
        memset(priorityTail, 0, sizeof(priorityTail));
        priorityCount[0] = ARRAYLEN(nodeInfo) - 1;
        priorityHead[Priority_Disconnected] = 1;
        priorityTail[Priority_Disconnected] = ARRAYLEN(nodeInfo) - 1;
        for (uint32_t i = 1; i < ARRAYLEN(nodeInfo); i++)
        {
            nodeInfo[i].prev = i - 1;
            nodeInfo[i].next = i + 1;
        }
        nodeInfo[ARRAYLEN(nodeInfo) - 1].next = 0;

        // Set up radio IRQ pin
        STM32::EXTI::configure(PIN_RADIO_NIRQ, STM32::EXTI::Config(true, false, STM32::EXTI::EDGE_FALLING));
        IRQ::enableRadioIRQ(true);
        IRQ::enableRadioTimerIRQ(true);

        // Start up the timer, it ticks once per bit and interrupts us once every frame.
        // Base clock frequency is 60 MHz.
        IRQ::enableRadioTimerIRQ(true);
        Timer::start(&RADIO_TIMER, RADIO_TIMER_CLK, 30 << channelAttrs->ca.speed, frameBits);
    }


    // Check if there is space in the command buffer, and if so, return a pointer for writing to the next slot.
    RF::Packet* getCommandBuffer()
    {
        // Do we have space in the buffer?
        int free = cmdReadPtr - cmdWritePtr - 1;
        if (free < 0) free += ARRAYLEN(cmdTarget);
        if (free) return cmdData + cmdWritePtr;
        return NULL;
    }


    // Enqueue the command written to the buffer (using getCommandBuffer) for a specific target
    void enqueueCommand(uint8_t target)
    {
        cmdTarget[cmdWritePtr] = target;
        if (cmdWritePtr + 1 == ARRAYLEN(cmdTarget)) cmdWritePtr = 0;
        else cmdWritePtr++;
    }


    // Get a pointer to the next received packet if there is one
    RF::Packet* getNextRxPacket()
    {
        int used = rxWritePtr - rxReadPtr;
        if (used < 0) used += ARRAYLEN(rxPacket);
        if (used) return rxPacket + rxReadPtr;
        return NULL;
    }


    // Release the buffer that was returned by getNextRxPacket back into the pool
    void releaseRxPacket()
    {
        if (rxReadPtr + 1 == ARRAYLEN(rxPacket)) rxReadPtr = 0;
        else rxReadPtr++;
    }

}
