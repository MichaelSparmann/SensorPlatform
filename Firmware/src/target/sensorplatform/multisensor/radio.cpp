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

// See thesis paper for theory of operation.


#include "global.h"
#include "radio.h"
#include "soc/stm32/gpio.h"
#include "soc/stm32/exti.h"
#include "cpu/arm/cortexm/cortexutil.h"
#include "sys/util.h"
#include "sys/time.h"
#include "../common/driver/timer.h"
#include "driver/spi.h"
#include "driver/dma.h"
#include "driver/random.h"
#include "driver/clock.h"
#include "common.h"
#include "irq.h"
#include "commands.h"
#include "sensortask.h"
#include "storagetask.h"
#include "interface/resetline/resetline.h"


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

    static const DMA::Config dmaRxCfg(RADIO_DMA_RX_PRIORITY, DMA::DIR_P2M, false,
                                      DMA::TS_8BIT, true, DMA::TS_8BIT, false, true);
    static const DMA::Config dmaTxCfg(RADIO_DMA_TX_PRIORITY, DMA::DIR_M2P, false,
                                      DMA::TS_8BIT, true, DMA::TS_8BIT, false, true);
    static const DMA::Config dummyTxCfg(RADIO_DMA_TX_PRIORITY, DMA::DIR_M2P, false,
                                        DMA::TS_8BIT, false, DMA::TS_8BIT, false, false);
    static const NRF::NRF24L01P::DataRate dataRateMapping[] =
    {
        NRF::NRF24L01P::DataRate_2Mbit,
        NRF::NRF24L01P::DataRate_1Mbit,
        NRF::NRF24L01P::DataRate_1Mbit,  // Invalid, reserved
        NRF::NRF24L01P::DataRate_250Kbit,
    };
    static int8_t dummyTx = -1;
    static uint8_t dummyRx;


    // Whether the radio was configured since the last shutdown (and is thus currently operating)
    static bool operating;

    // How many more timer ticks (milliseconds) we will wait for a beacon packet before we go to sleep
    static uint16_t beaconTimeout;

    // Beacon packet that describes the channel that we are a member of or trying to join
    static RF::Packet::Beacon beaconPacket;

    // The last received SOF packet
    static RF::Packet::SOF sofPacket;
    // The time and sequence information part of the previous received SOF packet
    static RF::Packet::SOF::Info lastSOFInfo;

    // The time at which the reception of the last SOF packet was completed
    static int frameStartTime;
    // Whether the last SOF packet's timestamp is likely to be accurate (no interfering IRQs).
    static bool frameStartTimeAccurate;

    // The time at which the reception of the previous SOF packet was completed
    static int previousFrameStartTime;
    // Whether the previous SOF packet's timestamp is likely to be accurate (no interfering IRQs).
    static bool previousFrameStartTimeAccurate;

    // If a radio IRQ is currently pending because it collided with a DMA transfer, this stores its timestamp.
    // Otherwise it is zero. This serves as a "packet download pending" flag. A timestamp of 0 will be incremented to 1.
    static int pendingIRQTime;
    // Whether the last radio IRQ's timestamp is likely to be accurate (no interfering IRQs).
    static bool pendingIRQTimeAccurate;

    // The local time at which the last IRQ handler completed, for SOF timing accuracy estimation
    static int lastIRQEnd;

    // Whether a DMA transfer is currently controlling the SPI bus (locking out IRQs)
    static bool dmaActive;

    // Whether received packets should be downloaded immediately as they arrive (SOF, Beacon)
    // or only if there is spare time as indicated by spiDeadline (others).
    static bool downloadImmediately;

    // Our own node ID. Zero if we don't have one.
    static uint8_t nodeId;
    static bool nodeIdChanged;
    // The local timestamp when out node ID will expire if we have no communication.
    static int nodeIdTimeout;

    // Index of the buffers transmitted during the slots of the last frame
    static int8_t lastFrameTxBuf[28];

    // Transmission buffers, will be used in unpredictable order. Contents are kept for retransmission until ACked.
    static RF::Packet::Reply txData[RADIO_TX_BUFFERS];
    static uint8_t txBufBeingRead;
    static uint8_t txBufBeingWritten;
    static struct __attribute__((packed,aligned(1)))
    {
        uint8_t attemptsLeft : 8;
    } txBufInfo[ARRAYLEN(txData)];
    // (At least) how many packets could currently be transmitted.
    static uint8_t txPending;
    // A rotating counter of enqueued TX packets, and its captured value at the last txPending counting time.
    // Used to estimate a lower bound on the additional number of packets enqueued since that time.
    static int8_t txSubmitCount;
    static int8_t capturedTxSubmitCount;

    // Notification transmission buffer
    static RF::Packet::Notify notifyData;
    static bool notifyPending;

    // Ring buffer of received packets and their target addresses
    static uint8_t rxPipe[RADIO_RX_BUFFERS];
    static int rxTime[ARRAYLEN(rxPipe)];
    static RF::Packet rxData[ARRAYLEN(rxPipe)];
    static uint8_t rxWritePtr;
    static uint8_t rxReadPtr;

    // The TX slot number within the frame that would require asserting CE at this timer tick
    static int16_t currentSlot;
    // The next TX slot number that we want to transmit data in
    static int8_t nextTxSlot;
    // The desired level of the CE line to be applied at the next slot boundary.
    static bool nextSlotCE;
    // The number of microseconds that our consecutive transmissions have
    // drifted ahead of the slot schedule due to guard bits.
    static uint8_t guardDrift;

    // The expected duration of a frame on the current channel
    static int frameUsecs;
    // The required RX time after an SOF packet on the current channel
    static int cmdUsecs;
    // The offset until the beginning of TX slots after an SOF packet on the current channel
    static int offsetUsecs;
    // The spacing between slots
    static int guardUsecs;
    // The duration of a slot on the current channel
    static int slotUsecs;
    // The maximum amount of clock deviation that we may accumulate in a frame without destroying adjacent slots
    static int maxJitterUsecs;
    // Whether we are confident that out oscillator is within maxJitterUsecs across one frame duration
    static bool oscillatorAccurate;

    // Current radio configuration, switches between PTX and PRX mode
    static NRF::NRF24L01P::Config radioCfg(NRF::Radio::Role_PTX, true, NRF::Radio::CrcMode_16Bit, true, true, false);

    static enum State
    {
        State_WaitForRx = 0,  // We are waiting for a data received or timer IRQ
        State_DownloadBeacon,  // Downloading received beacon packet, blocked on RX DMA
        State_DownloadSOF,  // Downloading received SOF packet, blocked on RX DMA
        State_DownloadMessage,  // Downloading received command, broadcast or notify packet, blocked on RX DMA
        State_UploadReply,  // Uploading reply packet, blocked on TX DMA
        State_UploadNoData,  // Uploading "we have no data to send" packet, blocked on TX DMA
        State_UploadNotify,  // Uplading notification packet, blocked on TX DMA
        State_SharedSPITransfer,  // A shared SPI bus transfer is in progress, blocked on RX DMA
    } currentState;

    // Deadline for shared SPI transactions during the current slot (if applicable)
    static int spiDeadline;

    // The amount of time that the enqueued shared SPI transaction would take.
    // If this is non-zero, it indicates that a transaction is currently waiting to be executed.
    static uint8_t spiRequiredTime;

    // Various attributes of the enqueued shared SPI transaction. Valid if spiRequiredTime is non-zero.
    static GPIO::Pin spiPin;
    static uint8_t spiPrescaler;
    static uint8_t spiLen;
    static const void* spiTxBuf;
    static void* spiRxBuf;
    static DMA::Config spiTxCfg;
    static DMA::Config spiRxCfg;
    static bool spiOldClockState;

    // Whether the radio DPCs are currently pending or running:
    static bool frameTaskRunning;
    static bool commandHandlerRunning;

    // Whether we are currently connected to a channel
    bool connected;

    // How many consecutive times we have listened for beacon packets without success.
    uint8_t failedAssocAttempts;

    // Delta between radio master and local microsecond timers
    int globalTimeOffset;

    // Current urgency level for transmission. Directly written by DPC code.
    uint8_t urgencyLevel;

    // The reply packet that will be sent if we have no other data to be sent in a slot that was reserved for us.
    // The various radio counters are updated internally. The rest ist written by DPC code.
    // The header and localTime fields will be updated internally before transmission.
    RF::Packet::Reply::NoData noDataResponse{{0, 0}, 0, RF::RID_NoData};


    // Send a command to the radio chip
    static NRF::SPI::Status sendCmd(uint8_t cmd)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, cmd);
        NRF::SPI::Status status(SPI::pullByte(&RADIO_SPI_BUS));
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
        return status;
    }


    // Write radio chip configuration register
    static NRF::SPI::Status writeReg(uint8_t reg, uint8_t data)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, NRF::SPI::Cmd_WriteReg | reg);
        SPI::pushByte(&RADIO_SPI_BUS, data);
        NRF::SPI::Status status(SPI::pullByte(&RADIO_SPI_BUS));
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
    }


    // Initiate DMA download of a received radio packet
    static void startPacketDownload(void* packet, size_t len)
    {
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, NRF::SPI::Cmd_ReadPacket);
        // Get the status byte out of the way, we don't want it to end up in the DMA receive buffer.
        SPI::pullByte(&RADIO_SPI_BUS);
        DMA::startTransferFromPri0(&RADIO_DMA_RX_REGS, dmaRxCfg, packet, len);
        DMA::startTransferFromPri0(&RADIO_DMA_TX_REGS, dummyTxCfg, &dummyTx, len);
        dmaActive = true;
    }


    // Configures the radio for beacon listening mode. This function can be called both from IRQ and DPC context.
    // If it is called from DPC contect, IRQs need to be locked out until it completes. Assumes that SPI is powered up.
    static void enterBeaconListenMode()
    {
        // Stop whatever we were doing, just listen for beacon packets now.
        GPIO::setLevelFast(PIN_RADIO_CE, false);
        radioCfg.b.role = NRF::Radio::Role_PRX;
        if (dmaActive) error(Error_RadioEnterBeaconModeDMACollision);
        writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(true, true, true).d8);  // Clear all IRQs
        sendCmd(NRF::SPI::Cmd_FlushTx);
        sendCmd(NRF::SPI::Cmd_FlushRx);
        writeReg(NRF::NRF24L01P::Reg_RfChannel, RF::BEACON_CHANNEL);
        writeReg(NRF::NRF24L01P::Reg_RfSetup, RF::BEACON_RF_SETUP.d8);
        writeReg(NRF::NRF24L01P::Reg_RxPipeEnable, NRF::NRF24L01P::RxPipeEnable(true, false, false, false, false, false).d8);
        writeReg(NRF::Radio::Reg_Config, radioCfg.d8);
        RADIO_SPI_OFF();

        // Start listening
        GPIO::setLevelFast(PIN_RADIO_CE, true);
        nextSlotCE = true;
GPIO::setLevelFast(PIN_LED, false);

        // Start up the timer, it will tick in millisecond intervals.
        // It will only handle shared SPI bus access until we join a channel.
        Timer::start(&RADIO_TIMER, RADIO_TIMER_CLK, 48, 1000);
        currentSlot = -3;

        nodeId = 0;
        connected = false;
        downloadImmediately = true;
        currentState = State_WaitForRx;
        operating = true;
        if (failedAssocAttempts < 255) failedAssocAttempts++;
    }


    static bool readIRQPin()
    {
        Clock::onFromPri0(STM32_GPIO_CLOCKGATE(STM32::GPIO::getPort(PIN_RADIO_NIRQ)));
        bool result = GPIO::getLevelFast(PIN_RADIO_NIRQ);
        Clock::offFromPri0(STM32_GPIO_CLOCKGATE(STM32::GPIO::getPort(PIN_RADIO_NIRQ)));
        return result;
    }


    // Check if there are pending RX packets and start downloading one if yes. Expects the SPI bus to be powered up.
    static bool checkReceived()
    {
        do
        {
            // Check for any data in the receive FIFO and clear the IRQ bit while doing so
            NRF::SPI::Status status = writeReg(NRF::Radio::Reg_Status, NRF::SPI::Status(false, false, true).d8);
            // If the RX FIFO is empty, we break out of the loop.
            if (status.b.rxPipe < 6)
            {
                // There is a packet in the receive FIFO, figure out if anyone wants to have it.
                if (!connected)
                    switch (status.b.rxPipe)  // We are not associated to a channel and listening for beacon packets.
                    {
                    case 0:  // This is a beacon packet, download it to the beacon buffer.
                        startPacketDownload(&beaconPacket, sizeof(beaconPacket));
                        currentState = State_DownloadBeacon;
                        connected = true;
                        return true;
                    default: break;  // We aren't interested in anything but beacons right now.
                    }
                else
                    switch (status.b.rxPipe)  // We are associated to a channel.
                    {
                    case 1:  // This is an SOF packet, download it to the SOF buffer.
                        startPacketDownload(&sofPacket, sizeof(sofPacket));
                        currentState = State_DownloadSOF;
                        frameStartTime = pendingIRQTime;
                        frameStartTimeAccurate = pendingIRQTimeAccurate;
GPIO::setLevelFast(PIN_LED, false);
                        return true;
                    case 2 ... 5:  // This is a command, broadcast or notifyReply packet.
                    {
                        // Reset node ID timeout if this is a non-broadcast command addressed to us
                        if (status.b.rxPipe == 5) nodeIdTimeout = TIMEOUT_SETUP(NODE_ID_TIMEOUT);
                        // Do we have space in our receive buffer for that?
                        int free = rxReadPtr - rxWritePtr - 1;
                        if (free < 0) free += ARRAYLEN(rxPipe);
                        if (free)
                        {
                            startPacketDownload(rxData + rxWritePtr, sizeof(*rxData));
                            rxPipe[rxWritePtr] = status.b.rxPipe;
                            rxTime[rxWritePtr] = pendingIRQTime;
                            currentState = State_DownloadMessage;
                            noDataResponse.telemetry.rxCmdCount++;
                            return true;
                        }
                        break;
                    }
                    default: break;  // We aren't interested in beacon packets.
                    }
                sendCmd(NRF::SPI::Cmd_FlushRx);  // Nobody is interested in this packet, so throw it away.
            }
        } while (!readIRQPin());
        // We have handled everything.
        pendingIRQTime = 0;
        return false;
    }


    // Initiate joining the channel advertized by the buffered beacon packet, if it isn't blacklisted.
    // Expects the SPI bus to be powered up.
    static void tryJoinChannel()
    {
        // Switch to the new frequency and bitrate and start listening
        GPIO::setLevelFast(PIN_RADIO_CE, false);
        NRF::NRF24L01P::RfSetup rfSetup(true, (NRF::NRF24L01P::Power)beaconPacket.channelAttrs.txPower,
                                        dataRateMapping[beaconPacket.channelAttrs.speed], false);
        writeReg(NRF::NRF24L01P::Reg_RfChannel, beaconPacket.channelAttrs.channel);
        writeReg(NRF::NRF24L01P::Reg_RfSetup, rfSetup.d8);
        GPIO::setLevelFast(PIN_RADIO_CE, true);
GPIO::setLevelFast(PIN_LED, true);

        // While the PLL is locking, figure out the timings.
        // Slot: 8 bits preamble, 24 bits address, 256 bits payload, 16 bits CRC
        int slotBits = 8 + 24 + 256 + 16;
        int cmdBits = slotBits * beaconPacket.channelAttrs.cmdSlots;
        guardUsecs = ((beaconPacket.channelAttrs.guardBits) << beaconPacket.channelAttrs.speed) >> 1;
        slotUsecs = ((slotBits + beaconPacket.channelAttrs.guardBits) << beaconPacket.channelAttrs.speed) >> 1;
        cmdUsecs = (cmdBits << beaconPacket.channelAttrs.speed) >> 1;
        offsetUsecs = (beaconPacket.channelAttrs.offsetBits << beaconPacket.channelAttrs.speed) >> 1;
        frameUsecs = offsetUsecs + slotUsecs * 28;
        maxJitterUsecs = (beaconPacket.channelAttrs.guardBits << beaconPacket.channelAttrs.speed) >> 2;
        oscillatorAccurate = false;

        // Set up the TX pipe for reply transmission
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_TxAddress);
        SPI::pushByte(&RADIO_SPI_BUS, RF::Address::Reply);
        SPI::pushByte(&RADIO_SPI_BUS, beaconPacket.channelAttrs.netId);
        SPI::pushByte(&RADIO_SPI_BUS, RF::PROTOCOL_ID);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);

        // Set up RX pipe 1 for SOF packet reception with the new network ID
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_RxPipeAddress1);
        SPI::pushByte(&RADIO_SPI_BUS, RF::Address::SOF);
        SPI::pushByte(&RADIO_SPI_BUS, beaconPacket.channelAttrs.netId);
        SPI::pushByte(&RADIO_SPI_BUS, RF::PROTOCOL_ID);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);

        // Enable the pipes that we need: SOF (1), NotifyReply (2), Broadcast (4)
        // Pipe 5 will be enabled once we are assigned an address on this channel.
        writeReg(NRF::NRF24L01P::Reg_RxPipeEnable, NRF::NRF24L01P::RxPipeEnable(false, true, true, false, true, false).d8);

        // Wait for an SOF packet for ~250ms. If we don't get one, switch back to listening for beacon packets.
        currentSlot = 1800;
        currentState = State_WaitForRx;
        failedAssocAttempts = 0;
    }


    // Figure out at what time we want to send our next packet, and start uploading it to the TX FIFO.
    static void prepareNextTx()
    {
        // Check if we may actually send anything during this frame (are we sure about slot timing alignment?)
        if (frameStartTimeAccurate && oscillatorAccurate)
        {
            // Check if we want to transmit something in a future slot of this frame
            for (uint32_t slot = nextTxSlot + 1; slot < ARRAYLEN(sofPacket.slot); slot++)
            {
                // Is this slot reserved for us?
                if (nodeId && sofPacket.slot[slot].owner == nodeId)
                {
                    // Reset node ID timeout
                    nodeIdTimeout = frameStartTime + NODE_ID_TIMEOUT;
                    noDataResponse.telemetry.txAttemptCount++;
                    // Do we have something to transmit?
                    if (txPending)
                    {
                        // Pick the next pending transmission buffer. If txPending != 0, there must be at least one.
                        uint32_t i = txBufBeingRead;
                        while (!txBufInfo[i].attemptsLeft)
                            if (++i >= ARRAYLEN(txData))
                                i = 0;
                        txBufBeingRead = i;
                        // Fill in packet header
                        txData[i].header.nodeId = nodeId;
                        int p = --txPending + ((txSubmitCount - capturedTxSubmitCount) & 0xff);
                        txData[i].header.bufferInfo = RF::Packet::Reply::Header::BufferInfo{(uint8_t)MIN(31, p),
                                                                                            urgencyLevel};
                        // Upload the packet
                        startPacketUpload(txData + i, sizeof(*txData));
                        lastFrameTxBuf[slot] = i;
                        // The DMA completion IRQ handler will take care of the rest
                        currentState = State_UploadReply;
                        nextTxSlot = slot;
                        return;
                    }
                    // We have a slot reserved for us, but nothing to send.
                    noDataResponse.header.nodeId = nodeId;
                    noDataResponse.header.bufferInfo = RF::Packet::Reply::Header::BufferInfo{0, urgencyLevel};
                    noDataResponse.localTime = read_usec_timer();
                    startPacketUpload(&noDataResponse, sizeof(noDataResponse));
                    lastFrameTxBuf[slot] = -1;
                    currentState = State_UploadNoData;
                    nextTxSlot = slot;
                    return;
                }
                else if (notifyPending && sofPacket.slot[slot].owner == RF::Address::Notify
                      && !(Random::random2K() & 0xff))
                {
                    // This is a notification slot that we want to make use of.
                    startPacketUpload(&notifyData, sizeof(notifyData));
                    lastFrameTxBuf[slot] = -1;
                    currentState = State_UploadNotify;
                    nextTxSlot = slot;
                    return;
                }
            }
        }

        // We don't have anything more to transmit during this frame
        nextTxSlot = -2;
    }


    // Execute deferred packet downloads and shared SPI bus transactions, as time permits.
    // Expects the SPI bus to be powered up, and will power it down if there's nothing to do.
    static void handleDeferredWork()
    {
        // If we have an active DMA transfer, we are busy right now,
        // and this function will be called again once that transfer completes.
        if (dmaActive) return;

        // Check if we have pending packet downloads (and enough time to handle one). If yes, take care of it.
        if (pendingIRQTime && (downloadImmediately || !TIMEOUT_EXPIRED(spiDeadline - 100)) && checkReceived()) return;

        // Check if we have a pending shared SPI transaction (and enough time to handle it).
        if (spiRequiredTime && (!operating || !TIMEOUT_EXPIRED(spiDeadline - spiRequiredTime)))
        {
            // Configure the SPI clock prescaler as requested
            SPI::setFrequency(&RADIO_SPI_BUS, spiPrescaler);
            // Select the slave and  start the DMA transfer
            spiOldClockState = Clock::getState(STM32_GPIO_CLOCKGATE(STM32::GPIO::getPort(spiPin)));
            Clock::onFromPri0(STM32_GPIO_CLOCKGATE(STM32::GPIO::getPort(spiPin)));
            GPIO::setLevelFast(spiPin, false);
            DMA::startTransferFromPri0(&RADIO_DMA_RX_REGS, spiRxCfg, spiRxBuf, spiLen);
            DMA::startTransferFromPri0(&RADIO_DMA_TX_REGS, spiTxCfg, const_cast<void*>(spiTxBuf), spiLen);
            dmaActive = true;
            currentState = State_SharedSPITransfer;
            return;
        }

        // Apparently we have nothing to do. Shut down the SPI bus.
        RADIO_SPI_OFF();
        lastIRQEnd = read_usec_timer();
    }


    void timerTick()
    {
        // Check if we need to start transmission
        GPIO::setLevelFast(PIN_RADIO_CE, nextSlotCE);

        Timer::acknowledgeIRQ(&RADIO_TIMER);
        RADIO_SPI_ON();

        switch (++currentSlot)
        {
        case -1:
            // We are at the end of the last command slot. We need to sync up for the first packet transmission.
            // Sync up the timer to tick every time we need to assert CE in order to send a packet into a slot.
            spiDeadline = frameStartTime + offsetUsecs - 157;
            Timer::updatePeriod(&RADIO_TIMER, spiDeadline + 10 - read_usec_timer());
            Timer::reset(&RADIO_TIMER);
            Timer::updatePeriod(&RADIO_TIMER, slotUsecs);
            IRQ::clearRadioTimerIRQ();
            // Switch to TX mode, we already have a packet to be sent in the FIFO
            if (dmaActive) error(Error_RadioPTXDMACollision);
            radioCfg.b.role = NRF::Radio::Role_PTX;
            GPIO::setLevelFast(PIN_RADIO_CE, false);
            writeReg(NRF::Radio::Reg_Config, radioCfg.d8);
            nextSlotCE = nextTxSlot == currentSlot + 1;
            guardDrift = 0;
            // Do not assert CE yet, the next timer tick will do that at the precisely right moment.
            // If we can't trust our timing, we will not transmit anything (but stay in PTX mode for power reasons).
            // In that case, skip a few slots to start listening for SOF packets a bit early (we need to re-sync).
            if (!frameStartTimeAccurate || !oscillatorAccurate) currentSlot += 300 / slotUsecs + 1;
            break;

        case 0 ... ARRAYLEN(sofPacket.slot) - 2:
            if (currentSlot == nextTxSlot)
            {
                // We are transmitting a packet right now. Prepare the next one, if there is one.
                if (dmaActive) error(Error_RadioPrepareNextTXDMACollision);
                prepareNextTx();
                // If the next transmission isn't in the next slot, or we need to re-lock our PLL to let some time slip,
                // we need to tell the radio to stop after the current packet.
                guardDrift += guardUsecs;
                // The following values might need tweaking if there are problems with long packet sequences.
#ifdef DEBUG
                if (guardDrift > 140)
#else
                if (guardDrift > 150)
#endif
                {
                    guardDrift = 0;
                    GPIO::setLevelFast(PIN_RADIO_CE, false);
                }
            }
            nextSlotCE = nextTxSlot == currentSlot + 1;
            spiDeadline += slotUsecs;
            break;

        case ARRAYLEN(sofPacket.slot) - 1:
        {
            // This is the last TX slot of the current frame.
            // Sync the timer to the actual transmission times in preparation for switching to PRX mode.
            // If we aren't sure about timing, start listening a bit early.
            // Set up the next timer tick for an SOF reception timeout.
            spiDeadline = frameStartTime + frameUsecs - (frameStartTimeAccurate && oscillatorAccurate ? 20 : 300);
            Timer::updatePeriod(&RADIO_TIMER, spiDeadline + 10 - read_usec_timer());
            Timer::reset(&RADIO_TIMER);
            Timer::updatePeriod(&RADIO_TIMER, 1000);
            IRQ::clearRadioTimerIRQ();
            nextSlotCE = false;
            // Check if we need to update our node ID.
            if (nodeIdChanged)
            {
                nodeIdChanged = false;
                writeReg(NRF::NRF24L01P::Reg_RxPipeAddress5, nodeId);
                writeReg(NRF::NRF24L01P::Reg_RxPipeEnable, NRF::NRF24L01P::RxPipeEnable(false, true, true, false, true, !!nodeId).d8);
            }
            break;
        }

        case ARRAYLEN(sofPacket.slot):
            // The next thing that we'll get is an SOF packet. Switch to PRX mode.
            radioCfg.b.role = NRF::Radio::Role_PRX;
            if (dmaActive) error(Error_RadioPRXDMACollision);
            writeReg(NRF::Radio::Reg_Config, radioCfg.d8);
            GPIO::setLevelFast(PIN_RADIO_CE, true);
            nextSlotCE = true;
            downloadImmediately = true;
            spiDeadline = read_usec_timer() + slotUsecs + 80;
GPIO::setLevelFast(PIN_LED, true);
            break;

        case ARRAYLEN(sofPacket.slot) + 1 ... 2047:  // We are ticking in milliseconds now, not slots.
            // The next SOF packet is overdue, we have probably missed it.
            spiDeadline = read_usec_timer() + 2000;
            break;

        case 2048:
            // The next SOF packet is massively overdue, apparently our base station has disappeared.
            // Try to find another one by listening for beacon packets for about a second.
            beaconTimeout = 1000;
            if (dmaActive) currentSlot = 2047;
            else enterBeaconListenMode();
            break;

        default:
            // We are in beacon reception mode and have no timing reference.
            // Just push the shared SPI access deadline forward every tick and check for timeouts.
            currentSlot = -3;
            spiDeadline = read_usec_timer() + 2000;
            if (!beaconTimeout)
            {
                // We haven't received a beacon packet in time, so let's go to sleep, if we are allowed to.
                if (!dmaActive && !frameTaskRunning && !commandHandlerRunning
                 && SensorTask::state == SensorTask::State_Idle && StorageTask::state == StorageTask::State_Idle)
                {
                    // Power down, mask all IRQs
                    GPIO::setLevelFast(PIN_RADIO_CE, false);
                    RADIO_SPI_ON();
                    writeReg(NRF::Radio::Reg_Config, NRF::Radio::Config(NRF::Radio::Role_PTX, false,
                                                                        NRF::Radio::CrcMode_None, true, true, true).d8);
                    RADIO_SPI_OFF();
                    Timer::stop(&RADIO_TIMER, RADIO_TIMER_CLK);
                    STM32::EXTI::enableIRQ(PIN_RADIO_NIRQ, false);
                    operating = false;
                    // Only other DPCs can get in between this and actual shutdown
                    IRQ::setPending(IRQ::DPC_PowerSleepTask);
                }
            }
            else beaconTimeout--;
        }
        // Check if there's anything else that we could do in spare time, if we have any.
        // This will also power down the SPI bus if it isn't needed anymore.
        handleDeferredWork();
    }


    void handleDMACompletion()
    {
        // A DMA transfer was completed, so we need to deselect the radio chip.
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);
        dmaActive = false;

        // If we download anything after completing a DMA transfer without another radio IRQ in between,
        // then we had multiple packets in the radio's FIFO and can't rely on IRQ timing.
        pendingIRQTimeAccurate = false;

        // If the radio is being powered down, this will catch some leftovers.
        if (operating || currentState == State_SharedSPITransfer)
        {
            // Figure out what we were doing
            switch (currentState)
            {
            case State_DownloadBeacon:
                // We got a beacon packet. Try to join the channel that it advertizes.
                // If that fails, we will just wait for another one.
                currentState = State_WaitForRx;
                tryJoinChannel();
                // Capture some entropy here...
                Random::seed(Random::random() ^ read_usec_timer());
                break;

            case State_DownloadSOF:
            {
                // Check if we missed an SOF packet
                bool consecutive = sofPacket.info.seq == ((lastSOFInfo.seq + 1) & 0xf)
                                && !TIME_AFTER(frameStartTime, previousFrameStartTime + 12 * frameUsecs);
                noDataResponse.telemetry.sofReceived++;
                if (!frameStartTimeAccurate || !oscillatorAccurate) noDataResponse.telemetry.sofTimingFailed++;
                if (!consecutive) noDataResponse.telemetry.sofDiscontinuity++;
                bool acked = false;
                if (nodeId)
                {
                    // We got an SOF packet. Check if that acknowledges any packets from us.
                    // The ACK bits are applicable to us if we have a node ID and didn't miss an SOF packet.
                    if (consecutive)
                        for (uint32_t i = 0; i < ARRAYLEN(lastFrameTxBuf); i++)
                            if (sofPacket.slot[i].ack)
                            {
                                if (lastFrameTxBuf[i] >= -1)
                                {
                                    if (lastFrameTxBuf[i] >= 0)
                                    {
                                        txBufInfo[lastFrameTxBuf[i]].attemptsLeft = 0;
                                        acked = true;
                                    }
                                    noDataResponse.telemetry.txAckCount++;
                                }
                            }
                            //else if (lastFrameTxBuf[i] >= 0) txBufInfo[lastFrameTxBuf[i]].attemptsLeft--;
                    // Check if our node ID is still valid
                    if (TIME_AFTER(frameStartTime, nodeIdTimeout))
                    {
                        nodeId = 0;
                        nodeIdChanged = true;
                    }
                }
                memset(lastFrameTxBuf, -2, sizeof(lastFrameTxBuf));
                // Count how many packets we want to transmit
                capturedTxSubmitCount = txSubmitCount;
                txPending = 0;
                for (uint32_t i = 0; i < ARRAYLEN(txBufInfo); i++)
                    if (txBufInfo[i].attemptsLeft)
                        txPending++;
                // Set up the first transmission if there is one
                nextTxSlot = -1;
                currentState = State_WaitForRx;
                prepareNextTx();
                currentSlot = -2;
                // Set up the timer to wake us up right after the end of the last command slot.
                downloadImmediately = false;
                spiDeadline = frameStartTime + cmdUsecs - 25;
                Timer::updatePeriod(&RADIO_TIMER, spiDeadline + 10 - read_usec_timer());
                Timer::reset(&RADIO_TIMER);
                IRQ::clearRadioTimerIRQ();
                // Check oscillator accuracy and trim if necessary
                if (consecutive && frameStartTimeAccurate && previousFrameStartTimeAccurate)
                    oscillatorAccurate = Clock::trim((sofPacket.info.time - lastSOFInfo.time) & 0xfffffff,
                                                     frameStartTime - previousFrameStartTime, maxJitterUsecs);
                // Keep track of SOF packet timing and sequence numbers to check for frame loss.
                lastSOFInfo = sofPacket.info;
                previousFrameStartTime = frameStartTime;
                previousFrameStartTimeAccurate = frameStartTimeAccurate;
                if (frameStartTimeAccurate) globalTimeOffset = sofPacket.info.time - frameStartTime;
                // Trigger FrameTask DPC
                frameTaskRunning = true;
                IRQ::setPending(IRQ::DPC_RadioFrameTask);
                // If we have freed up transmission buffer space, trigger the command handler task as well.
                if (acked) IRQ::setPending(IRQ::DPC_RadioCommandHandler);
                break;
            }

            case State_DownloadMessage:
                // We got a command packet. Hand it off into the buffer.
                if (rxWritePtr + 1 == ARRAYLEN(rxPipe)) rxWritePtr = 0;
                else rxWritePtr++;
                currentState = State_WaitForRx;
                // Trigger CommandHandler DPC
                commandHandlerRunning = true;
                IRQ::setPending(IRQ::DPC_RadioCommandHandler);
                break;

            case State_UploadReply:
                // We just uploaded a reply packet, move to the next one.
                if (++txBufBeingRead >= ARRAYLEN(txData)) txBufBeingRead = 0;
                currentState = State_WaitForRx;
                break;

            case State_UploadNotify:
                // We just uploaded a notification packet, mark it as sent.
                notifyPending = false;
                currentState = State_WaitForRx;
                break;

            case State_UploadNoData:
                // We just uploaded a "we have no data" packet. Nothing else needs to be done.
                currentState = State_WaitForRx;
                break;

            case State_SharedSPITransfer:
                // We just finished a shared SPI transfer. Deselect the slave, set the clock prescaler to the
                // correct value for radio accesses and signal completion to the initiator of the transfer.
                GPIO::setLevelFast(spiPin, true);
                if (!spiOldClockState) Clock::offFromPri0(STM32_GPIO_CLOCKGATE(STM32::GPIO::getPort(spiPin)));
                SPI::setFrequency(&RADIO_SPI_BUS, RADIO_SPI_PRESCALER);
                spiRequiredTime = 0;
                IRQ::wakeSensorTask();
                currentState = State_WaitForRx;
                break;

            default: error(Error_RadioDMACompletionBadState);
            }
        }

        // Check if there's anything else that we could do in spare time, if we have any.
        // This will also power down the SPI bus if it isn't needed anymore.
        handleDeferredWork();
    }


    void handleIRQ()
    {
        // Capture IRQ timestamp
        pendingIRQTime = read_usec_timer();
        if (!pendingIRQTime) pendingIRQTime = 1;
        pendingIRQTimeAccurate = false;

        // Clear nIRQ line edge trigger
        STM32::EXTI::clearPending(PIN_RADIO_NIRQ);

        // If DMA is currently controlling the SPI bus, that will take care of the IRQ once it's finished.
        if (dmaActive) return;

        // Check whether there was likely no other IRQ interfering with timing measurements.
        if (pendingIRQTime - lastIRQEnd >= 10) pendingIRQTimeAccurate = true;

        // Check if we should postpone downloading this packet to keep timer ticks accurate.
        if (!downloadImmediately && TIME_AFTER(pendingIRQTime + 100, spiDeadline)) return;

        // Download received packets now.
        RADIO_SPI_ON();
        if (!checkReceived()) RADIO_SPI_OFF();
        lastIRQEnd = read_usec_timer();
    }


    ////// Everything above this line is running in IRQ context (priority 0) /////
    //////////////////////////////////////////////////////////////////////////////
    ////// Everything below this line is running in DPC context (priority 3) /////


    // When the last attempt to acquire a nodeId was made
    int lastIdentAttempt;

    // Whether a measurement is currently in progress and producing data that we need to send.
    bool measuring;
    // Whether the currently running measurement has completed producing data.
    // If this is set, we shall clear the measuring flag as soon as we have finished sending all data from the buffer.
    bool seriesComplete;
    // How many data pages we have skipped due to buffer overflows.
    uint32_t bufferOverflowLost;
    // The sequence number of the block that we're currently transmitting data from.
    static uint32_t currentBlockSeq;
    // The block index within the buffer that we're currently transmitting data from.
    static uint8_t currentBlock;
    // The page index within the current block that will be transmitted next.
    static uint8_t currentPage;


    void init()
    {
        // Set up the DMA controller for radio SPI bus access
        DMA::setPeripheralAddr(&RADIO_DMA_RX_REGS, SPI::getDataRegPtr(&RADIO_SPI_BUS));
        DMA::setPeripheralAddr(&RADIO_DMA_TX_REGS, SPI::getDataRegPtr(&RADIO_SPI_BUS));

        // Keep the relevant GPIO controllers awake
        GPIO::enableFast(PIN_RADIO_NCS, true);
        GPIO::enableFast(PIN_RADIO_CE, true);

        // Elevate to priority 0 (RADIO_SPI_ON may not be preempted)
        enter_critical_section();

        // Configure the SPI bus
        RADIO_SPI_ON();
        SPI::init(&RADIO_SPI_BUS);
        SPI::setFrequency(&RADIO_SPI_BUS, RADIO_SPI_PRESCALER);
        writeReg(NRF::Radio::Reg_Config, NRF::Radio::Config(NRF::Radio::Role_PTX, false, NRF::Radio::CrcMode_None,
                                                            true, true, true).d8);  // Power down, mask all IRQs
        RADIO_SPI_OFF();

        // Set up radio IRQ pin
        STM32::EXTI::configure(PIN_RADIO_NIRQ, STM32::EXTI::Config(false, false, STM32::EXTI::EDGE_FALLING));

        leave_critical_section();
    }


    void shutdown()
    {
        if (!operating) return;
        operating = false;

        // Disable IRQs to prevent any race conditions here
        Timer::stop(&RADIO_TIMER, RADIO_TIMER_CLK);
        STM32::EXTI::enableIRQ(PIN_RADIO_NIRQ, false);

        // Wait for any outstanding radio DMA transfers to finish
        while (!GPIO::getLevelFast(PIN_RADIO_NCS));

        // Shut down the radio, we need to elevate to priority 0 to avoid races with shared SPI.
        GPIO::setLevelFast(PIN_RADIO_CE, false);
        do
        {
            enter_critical_section();
            if (!dmaActive) break;
            leave_critical_section();
        } while (false);
        RADIO_SPI_ON();
        writeReg(NRF::Radio::Reg_Config, NRF::Radio::Config(NRF::Radio::Role_PTX, false, NRF::Radio::CrcMode_None,
                                                            true, true, true).d8);  // Power down, mask all IRQs
        RADIO_SPI_OFF();
        leave_critical_section();
    }


    void startup()
    {
        // Ensure that everything is stopped and that we're safe to modify the settings
        shutdown();

        // Elevate to priority 0 to avoid races with shared SPI.
        GPIO::setLevelFast(PIN_RADIO_CE, false);
        do
        {
            enter_critical_section();
            if (!dmaActive) break;
            leave_critical_section();
        } while (false);

        // Shut down the radio chip and configure it
        RADIO_SPI_ON();
        // Power down
        writeReg(NRF::Radio::Reg_Config, 0);
        // Get rid of Enhanced ShockBurst
        writeReg(NRF::NRF24L01P::Reg_FeatureCtl, NRF::NRF24L01P::FeatureCtl(false, false, false).d8);
        writeReg(NRF::NRF24L01P::Reg_AutoAckCtl, NRF::NRF24L01P::AutoAckCtl(false, false, false, false, false, false).d8);
        writeReg(NRF::NRF24L01P::Reg_DynLengthCtl, NRF::NRF24L01P::DynLengthCtl(false, false, false, false, false, false).d8);
        writeReg(NRF::NRF24L01P::Reg_RetransCtl, NRF::NRF24L01P::RetransCtl(0, 0).d8);
        // Configure address width
        writeReg(NRF::NRF24L01P::Reg_AddressCtl, NRF::NRF24L01P::AddressCtl(NRF::NRF24L01P::Width_24Bit).d8);
        // No need to power up the radio here yet, switching to beacon reception mode will do that later.

        // Set up all pipes other than pipe 0 for 32 byte packets.
        for (int i = 1; i < 6; i++) writeReg(NRF::Radio::Reg_RxDataLength0 + i, 32);

        // Set up RX pipe 0 for beacon packet reception
        writeReg(NRF::Radio::Reg_RxDataLength0, sizeof(beaconPacket));
        GPIO::setLevelFast(PIN_RADIO_NCS, false);
        SPI::pushByte(&RADIO_SPI_BUS, (NRF::SPI::Cmd_WriteReg & 0xff) | NRF::Radio::Reg_RxPipeAddress0);
        SPI::pushByte(&RADIO_SPI_BUS, RF::Address::Beacon);
        SPI::pushByte(&RADIO_SPI_BUS, RF::BEACON_NET_ID);
        SPI::pushByte(&RADIO_SPI_BUS, RF::PROTOCOL_ID);
        SPI::waitDone(&RADIO_SPI_BUS);
        GPIO::setLevelFast(PIN_RADIO_NCS, true);

        // RX pipe 1 will be set up for SOF packets by joinChannel when we know our netId

        // Set up RX pipe 2 for notify reply packet reception
        writeReg(NRF::Radio::Reg_RxPipeAddress2, RF::Address::NotifyReply);

        // RX pipe 3 is not used.

        // Set up RX pipe 4 for broadcast packet reception
        writeReg(NRF::Radio::Reg_RxPipeAddress4, RF::Address::Broadcast);

        // RX pipe 5 will be used for our own address, which we don't know yet.

        // Reset various internal state
        rxWritePtr = 0;
        rxReadPtr = 0;
        txBufBeingWritten = 0;
        txPending = 0;
        for (uint32_t i = 0; i < ARRAYLEN(txBufInfo); i++) txBufInfo[i].attemptsLeft = 0;
        for (uint32_t i = 0; i < ARRAYLEN(lastFrameTxBuf); i++) lastFrameTxBuf[i] = -1;
        notifyPending = 0;
        dmaActive = false;
        pendingIRQTime = 0;
        previousFrameStartTimeAccurate = false;
        spiDeadline = read_usec_timer();

        // Start listening for beacon packets
        STM32::EXTI::enableIRQ(PIN_RADIO_NIRQ, true);
        beaconTimeout = 18;
        enterBeaconListenMode();

        leave_critical_section();
    }


    // Check if there is space in a transmission buffer, and if so, return a pointer for writing to an empty buffer.
    RF::Packet::Reply* getFreeTxBuffer(int reserveSlots)
    {
        uint32_t i = txBufBeingWritten;
        while (++i != txBufBeingWritten)
        {
            if (i >= ARRAYLEN(txData) - reserveSlots) i = 0;
            if (txBufInfo[i].attemptsLeft) continue;
            txBufBeingWritten = i;
            return txData + i;
        }
        return NULL;
    }


    // Enqueue the command written to the buffer (using getCommandBuffer)
    void enqueuePacket(int maxAttempts)
    {
        typeof(*txBufInfo) info;
        info.attemptsLeft = maxAttempts;
        txBufInfo[txBufBeingWritten] = info;
        txSubmitCount++;
    }


    // Check if the notification buffer is free, and if so, return a pointer for writing to it.
    RF::Packet::Notify* getNotificationBuffer()
    {
        if (!notifyPending) return &notifyData;
        return NULL;
    }


    // Enqueue the notification written to the buffer (using getNotificationBuffer)
    void enqueueNotification()
    {
        notifyPending = true;
    }


    // Get a pointer to the next received packet if there is one, and fill in the pipe and time info
    RF::Packet* getNextRxPacket(uint8_t* pipePtr, int* timePtr)
    {
        int used = rxWritePtr - rxReadPtr;
        if (used < 0) used += ARRAYLEN(rxPipe);
        if (!used) return NULL;
        if (pipePtr) *pipePtr = rxPipe[rxReadPtr];
        if (timePtr) *timePtr = rxTime[rxReadPtr];
        return rxData + rxReadPtr;
    }


    // Release the buffer that was returned by getNextRxPacket back into the pool
    void releaseRxPacket()
    {
        if (rxReadPtr + 1 == ARRAYLEN(rxPipe)) rxReadPtr = 0;
        else rxReadPtr++;
    }


    // Execute an SPI transfer to another slave on a bus shared with the radio chip
    void sharedSPITransfer(GPIO::Pin pin, uint8_t prescaler, const void* txBuf, void* rxBuf, uint8_t len)
    {
        int duration = ((len * 8) << prescaler) / 24 + 20;
        if (duration > 255) return;
        spiPin = pin;
        spiPrescaler = prescaler;
        if (txBuf)
        {
            spiTxBuf = txBuf;
            new(&spiTxCfg) DMA::Config(RADIO_DMA_TX_PRIORITY, DMA::DIR_M2P, false,
                                       DMA::TS_8BIT, true, DMA::TS_8BIT, false, false);
        }
        else
        {
            spiTxBuf = &dummyTx;
            new(&spiTxCfg) DMA::Config(RADIO_DMA_TX_PRIORITY, DMA::DIR_M2P, false,
                                       DMA::TS_8BIT, false, DMA::TS_8BIT, false, false);
        }
        if (rxBuf)
        {
            spiRxBuf = rxBuf;
            new(&spiRxCfg) DMA::Config(RADIO_DMA_RX_PRIORITY, DMA::DIR_P2M, false,
                                       DMA::TS_8BIT, true, DMA::TS_8BIT, false, true);
        }
        else
        {
            spiRxBuf = &dummyRx;
            new(&spiRxCfg) DMA::Config(RADIO_DMA_RX_PRIORITY, DMA::DIR_P2M, false,
                                       DMA::TS_8BIT, false, DMA::TS_8BIT, false, true);
        }
        spiLen = len;
        spiRequiredTime = duration;
        while (spiRequiredTime)
        {
            // If shared SPI transactions would in theory currently be possible, try to elevate to priority 0.
            if (!dmaActive && (!operating || !TIMEOUT_EXPIRED(spiDeadline - spiRequiredTime)))
            {
                enter_critical_section();
                RADIO_SPI_ON();
                handleDeferredWork();
                leave_critical_section();
            }
            SensorTask::yield();
        }
    }


    // Sends a nodeId notification packet
    static void sendNodeIdNotification()
    {
        RF::Packet::Notify* msg = getNotificationBuffer();
        if (!msg) return;
        msg->header.channel = RF::Notify;
        msg->header.messageId = RF::NID_NodeId;
        msg->nodeId.reserved = 0;
        msg->nodeId.nodeId = nodeId;
        memcpy(&msg->nodeId.ident, &config.nodeUniqueId, sizeof(msg->nodeId.ident));
        Radio::enqueueNotification();
    }


    // Initiates the transmission of measurement data.
    // Stop transmission after the measurement is complete by setting seriesComplete to true,
    // and wait for measuring == false before discarding mainBuf contents.
    void startMeasurementTransmission()
    {
        currentBlockSeq = 0;
        currentBlock = 0;
        currentPage = 0;
        seriesComplete = false;
        measuring = true;
    }


    // Advance to the next block in the measurement transmission buffer.
    static void nextBlock()
    {
        bufferOverflowLost += ARRAYLEN(*mainBuf.block) - currentPage;
        if (++currentBlock >= ARRAYLEN(mainBuf.block)) currentBlock = 0;
        currentBlockSeq++;
        currentPage = 0;
    }


    // This function is called asynchronously after receiving an SOF pcaket.
    // If it takes longer than a frame to execute, calls will be skipped.
    void dpcFrameTask()
    {
        int now = read_usec_timer();

        // Check if we need to attempt to acquire a nodeId
        if (!nodeId && ((unsigned)(now - lastIdentAttempt)) > IDENT_ATTEMPT_INTERVAL)
        {
            lastIdentAttempt = now;
            sendNodeIdNotification();
        }

        // Check if we have measurement data to send
        if (measuring)
        {
            while (true)
            {
                urgencyLevel = MIN(7, 7 * (SensorTask::writeSeq - currentBlockSeq) / ARRAYLEN(mainBuf.block));
                RF::Packet::Reply* reply = getFreeTxBuffer(RADIO_TX_BUFFER_RESERVE);
                // No transmission buffer space available
                if (!reply) break;
                // No untransmitted data available
                if (mainBufSeq[currentBlock] < currentBlockSeq)
                {
                    if (seriesComplete)
                    {
                        measuring = false;
                        Radio::noDataResponse.bitrate = 0;
                        Radio::noDataResponse.dataSeq = 0;
                        IRQ::wakeSensorTask();
                    }
                    break;
                }
                // Buffer overflow, block was overwritten
                if (!mainBufValid[currentBlock] || mainBufSeq[currentBlock] != currentBlockSeq)
                {
                    nextBlock();
                    continue;
                }
                // Grab a copy of the data to be sent
                memcpy(reply->measurementData.data, &mainBuf.block[currentBlock][currentPage], sizeof(Page));
                // Check if the buffer overflowed while we were copying the packet
                if (!mainBufValid[currentBlock] || mainBufSeq[currentBlock] != currentBlockSeq)
                {
                    nextBlock();
                    continue;
                }
                reply->measurementData.seq = (currentBlockSeq * ARRAYLEN(*mainBuf.block) + currentPage) & 0x7fff;
                enqueuePacket(TX_ATTEMPTS_DATA);
                if (++currentPage >= ARRAYLEN(*mainBuf.block)) nextBlock();
                Radio::noDataResponse.dataSeq = currentBlockSeq * ARRAYLEN(*mainBuf.block);
            }
        }

        frameTaskRunning = false;
    }


    // This function is called after receiving one or more command, notifyReply or broadcast packets.
    // If further packets arrive while it executes, it will be re-run after it returns.
    void dpcCommandHandler()
    {
        RF::Packet* packet;
        uint8_t pipe;
        int time;

        // We need to process all command packets in the buffer
        while ((packet = getNextRxPacket(&pipe, &time)))
        {
            bool release = true;
            switch (pipe)
            {
            case 2:  // NotifyReply
                if (packet->notifyReply.header.channel != RF::NotifyReply) break;
                switch (packet->notifyReply.header.messageId)
                {
                case RF::NID_SetNodeId:
                    // Check if this is addressed to our unique ID
                    if (packet->notifyReply.setNodeId.hwId.vendor != config.nodeUniqueId.hardware.vendor
                     || packet->notifyReply.setNodeId.hwId.product != config.nodeUniqueId.hardware.product
                     || packet->notifyReply.setNodeId.hwId.serial != config.nodeUniqueId.hardware.serial) break;
                    // Set our node ID as requested, and acknowledge it.
                    nodeId = packet->notifyReply.setNodeId.nodeId;
                    nodeIdChanged = true;
                    nodeIdTimeout = time + NODE_ID_TIMEOUT;
                    sendNodeIdNotification();
                    break;

                default: break;
                }
                break;

            case 4:  // Broadcast
            case 5:  // Command
                release = Commands::handlePacket(&packet->cmd);
                break;
            }
            if (release) releaseRxPacket();
            else break;
        }

        commandHandlerRunning = false;
    }
}
