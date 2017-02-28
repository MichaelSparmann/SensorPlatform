#pragma once

// SensorPlatform Radio Protocol Definitions
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


namespace RF
{
    const uint8_t BEACON_CHANNEL = 82;  // Frequency of the beacon channel: (2400 + x) MHz
    // Beacon channel modulation parameters
    const NRF::NRF24L01P::RfSetup BEACON_RF_SETUP(true, NRF::NRF24L01P::Power_0dBm,
                                                  NRF::NRF24L01P::DataRate_1Mbit, false);
    const uint8_t PROTOCOL_ID = 0xec;  // Protocol ID magic number
    const uint8_t BEACON_NET_ID = 0xdd;  // Beacon channel NetID
    
    // These address values are used both as packet addresses for command packets
    // and as NodeIDs in slot assignments and RX packets.
    enum Address
    {
        Beacon = 0x00,  // Radio address 0x00 on BEACON_NET_ID
        Reply = 0x00,  // Radio address 0x00 on other NET_IDs
        MinNode = 0x01,  // Beginning of radio address and node ID range for nodes
        MaxNode = 0x64,  // End (inclusive) of radio address and node ID range for nodes
        // Gap reserved for future expansion
        Notify = 0x7f,  // Radio address 0x00 (node to receiver), node ID 0x7f
        NotifyReply = 0x7f,  // Radio address 0x7f (receiver to node), node ID 0x7f
        // Addresses >=0x80 are only usable as radio addresses, not as NodeIDs in packets
        Dummy = 0xfd,  // Dummy packet to start up transmitter, shall be ignored
        Broadcast = 0xfe,  // Radio address 0xfe, no node ID in packet (not used so far)
        SOF = 0xff,  // Radio address 0xff, no node ID in packet
    };

    // RF channel attributes (transmission/modulation parameters)
    struct __attribute__((packed,aligned(4))) ChannelAttributes
    {
        uint8_t channel : 7;  // Channel frequency: (2400 + x) MHz
        uint32_t : 1;
        uint8_t netId;  // Channel NetID (part of packet addresses)
        uint16_t offsetBits : 14;  // Bit times from end of SOF to start of first RX slot
        uint8_t speed : 2;  // GFSK modulation speed: 0: 2Mbps, 1: 1Mbps, 2: reserved, 3: 250kbps
        uint8_t guardBits;  // Bit times of turnaround time between RX slots
        uint8_t cmdSlots : 4;  // (Maximum) number of command slots after SOF
        uint8_t txPower : 2;  // Desired sensor node transmission power: (6 * x) - 18 dBm
        uint32_t : 18;
    };

    // Additional RF channel attributes only relevant to the base station
    struct __attribute__((packed,aligned(4))) ExtendedChannelAttributes
    {
        ChannelAttributes ca;
        uint32_t : 32;
        uint8_t tailBits;  // Additional gap bits between frames
        uint8_t receiverTxPower : 2;  // Desired base station transmission power: (6 * x) - 18 dBm
        uint8_t : 6;
        uint8_t reserved[18];
    };

    // Internal time slot assignment data structure
    struct __attribute__((packed,aligned(1))) TimeSlotOwner
    {
        uint8_t owner : 7;  // NodeID of the slot owner
        bool sticky : 1;  // 0: Just for one frame, 1: Permanent assignment
    };

    // Bytes 3-4 of received (non-notify) messages contain a 16 bit wide header field,
    // which identifies the packet type:
    //     MSB . . . . . . . . . . . . LSB (LITTLE ENDIAN!)
    //     0 s s s s s s s s s s s s s s s: Payload data packet (s: sequence number bits)
    //     1 0 0 s s s s s r r r r r r r r: Command response (s: sequence number, r: result code)
    //     1 0 1 x x x x x x x x x x x x x: Reserved for future expansion
    //     1 1 0 x x x x x x x x x x x x x: Reserved for future expansion
    //     1 1 1 1 1 1 1 1 n n n n n n n n: No data ready for transmission, poll again in n frames
    enum ReplyMessageType  // Highest 3 bits of ReplyMessageId
    {
        RT_CommandResponse = 0b100,
        RT_SpecialMessage = 0b111,
    };

    enum ReplyMessageId
    {
        RID_NoData = 0xff,
    };

    // Message IDs for notification packets (byte 2)
    // The high bit indicates the direction (0: node to host, 1: host to node)
    enum NotifyMessageId
    {
        NID_NodeId = 0x00,  // Current NodeID notification (issued by sensor nodes)
        NID_SetNodeId = 0x80,  // NodeID change request (issued by the host)
    };

    // Sensor node command IDs
    enum CommandId
    {
        CID_ReadPageConfig = 0x0100,  // Read node config page
        CID_WritePageConfig = 0x0101,  // Write node config page
        CID_ReadPageSeries = 0x0102,  // Read series header page (not the sensor specific part)
        CID_WritePageSeries = 0x0103,  // Write series header page (not the sensor specific part)
        CID_ReadPageSensor = 0x0104,  // Read sensor attribute page
        CID_WritePageSensor = 0x0105,  // Write sensor attribute page
        CID_SaveConfig = 0x0106,  // Save node config pages to flash
        CID_SaveSeriesHeader = 0x0107,  // Save series header pages (including sensors) to flash
        CID_StartMeasurement = 0x0110,  // Start measurement (as configured by series header)
        CID_StopMeasurement = 0x0111,  // Stop measurement (returns OK of none is running)
        CID_StartUpload = 0x01f0,  // Switch to firmware upload mode
        CID_StopUpload = 0x01f1,  // Leave firmware upload mode (returns OK if not in upload mode)
        CID_UploadData = 0x01f2,  // Transfer 28-byte firmware chunk to sensor node
        CID_WriteSector = 0x01f3,  // Write 512-byte firmware block to microSD card
        CID_UpgradeFirmware = 0x01f4,  // Initiate firmware upgrade using updater in sector buffer
        CID_Reboot = 0x01ff,  // Reboot the sensor node firmware
    };

    // Sensor node command return codes
    enum Result
    {
        Result_OK = 0x00,
        Result_UnknownError = 0x01,
        Result_UnknownCommand = 0x02,
        Result_InvalidArgument = 0x03,
        Result_HandlerFailed = 0x04,
        Result_Busy = 0x05,
    };

    // Globally unique hardware ID (HwId) of a device
    struct __attribute__((packed,aligned(4))) HwUniqueId
    {
        uint32_t vendor;
        uint32_t product;
        uint32_t serial;
    };

    // Globally unique protocol ID (ProtoId) understood by a device
    struct __attribute__((packed,aligned(4))) ProtocolId
    {
        uint32_t vendor;
        uint16_t type;
        uint16_t version;
    };

    // Globally unique firmware identification (FwId) including version
    struct __attribute__((packed,aligned(4))) FwVersion
    {
        uint32_t vendor;
        uint16_t type;
        uint16_t version;
    };

    // Full RF node instance and type information (used in NID_NodeId packets)
    struct __attribute__((packed,aligned(4))) NodeIdentification
    {
        HwUniqueId hardware;
        ProtocolId protocol;
        FwVersion firmware;
    };

    // Sensor node telemetry data (transmitted as part of RID_NoData packets)
    struct __attribute__((packed,aligned(4))) TelemetryData
    {
        uint16_t sofReceived;  // Total number of SOF packets received
        uint16_t sofTimingFailed;  // SOF packets received with bad/unreliable timing info
        uint16_t sofDiscontinuity;  // SOF packet loss events (not a packet count)
        uint16_t txAttemptCount;  // Number of transmitted packets
        uint16_t txAckCount;  // Number of acknowledged packets
        uint16_t rxCmdCount;  // Number of received non-SOF packets
        uint16_t reserved[2];  // Reserved for battery state etc.
    };

    // Radio packet structure
    union __attribute__((packed,aligned(4))) Packet
    {
        // Beacon packets announce the presence of a base station on a well-known frequency
        struct __attribute__((packed,aligned(4))) Beacon
        {
            ChannelAttributes channelAttrs;  // Parameters of the data channel of that base station
            uint32_t : 32;
            uint32_t time : 28;  // Microsecond counter of that base station (for calibration)
            uint32_t seq : 4;  // Beacon packet counter of that base station
        } beacon;

        // Start Of Frame (SOF) packet structure
        struct __attribute__((packed,aligned(4))) SOF
        {
            // Sensor node => host (RX) time slot assignments
            struct __attribute__((packed,aligned(1))) TimeSlot
            {
                uint8_t owner : 7;  // Slot owner NodeId for this frame
                bool ack : 1;  // Acknowledgement of correct packet reception in previous frame
            } slot[28];
            struct __attribute__((packed,aligned(4))) Info
            {
                uint32_t time : 28;  // Microsecond counter of base station (for calibration/sync)
                uint32_t seq : 4;  // SOF packet counter of that base station (for loss detection)
            } info;
        } sof;

        // Host => sensor node command packets
        union __attribute__((packed,aligned(4))) Command
        {
            // Every pachet starts with this header structure
            struct __attribute__((packed,aligned(2))) Header
            {
                CommandId cmd : 16;  // Command ID
                uint8_t arg;  // Command argument (e.g. page number that should be accessed)
                uint8_t seq : 5;  // Command sequence number (echoed back in response)
                uint8_t : 3;
            } header;

            // CID_* commands not explicitly listed below use just the header

            // Host => sensor node data transfer packets can have these CIDs:
            //     CID_WritePageConfig: Write to node configuration data
            //     CID_WritePageSeries: Write to series header (non-sensor) data
            //     CID_WritePageSensor: Write to sensor attribute data
            //     CID_UploadData: Write to SD sector buffer (for firmware upgrade)
            // The index of the page to write to is contained in the header arg field.
            struct __attribute__((packed,aligned(4))) WritePage
            {
                Header header;  // CID_WritePage* or CID_UploadData
                uint8_t data[28];
            } writePage;

            // Initiates a measurement
            struct __attribute__((packed,aligned(4))) StartMeasurement
            {
                Header header;  // CID_StartMeasurement
                uint32_t atGlobalTime;  // Base station microsecond time to start measurement at
                uint64_t unixTime;  // Unix timestamp to be put into the series header
            } startMeasurement;

            // Write sector buffer contents (transferred using CID_UploadData) to SD card
            struct __attribute__((packed,aligned(4))) WriteSector
            {
                Header header;  // CID_WriteSector
                uint32_t sector;  // SD card sector number (from start of data section)
            } writeSector;

            // Initiate firmware upgrade. Firmware image must have been uploaded to the SD card
            // and the sector buffer must contain the updater code, which will be executed by
            // this command. After acknowledging the command, the sensor node will reboot into
            // the uploaded updater code, which will erase the firmware flash and program it
            // with data from the SD card if the CRC matches. The node then reboots.
            struct __attribute__((packed,aligned(4))) UpgradeFirmware
            {
                Header header;  // CID_UpgradeFirmware
                uint32_t size;  // Firmware image size (in 512-byte sectors)
                uint32_t crc;  // Firmware image CRC32 (upgrade will be aborted if incorrect)
            } upgradeFimware;
        } cmd;

        // Sensor node => host packets (command replies or measurement data)
        union __attribute__((packed,aligned(4))) Reply
        {
            // Every pachet starts with this header structure
            struct __attribute__((packed,aligned(2))) Header
            {
                uint8_t nodeId;  // NodeId of the transmitting node
                struct __attribute__((packed,aligned(1))) BufferInfo
                {
                    uint8_t pendingPackets : 5;  // Number of packets ready for transmission
                    uint8_t urgency : 3;  // Transmission urgency (based on buffer level)
                } bufferInfo;
            } header;

            // Measurement data packet
            struct __attribute__((packed,aligned(4))) MeasurementData
            {
                Header header;
                uint16_t seq;  // High bit must be zero to indicate packet type!
                uint8_t data[28];
            } measurementData;

            // No data available to be transmitted status packet
            struct __attribute__((packed,aligned(4))) NoData
            {
                Header header;
                uint8_t pollInFrames;  // Node requests to be polled again n frames later
                ReplyMessageId messageId : 8;  // RID_NoData
                uint32_t localTime;  // Current microsecond timer value of the node
                uint32_t bitrate;  // Estimated average measurement data bitrate (not implemented)
                uint32_t dataSeq;  // Highest measurement data packet sequence number sent so far
                TelemetryData telemetry;  // Radio link and node status telemetry data
            } noData;

            // Command reply packet header
            struct __attribute__((packed,aligned(4))) CommandReplyHeader
            {
                Header header;
                Result result : 8;
                uint8_t seq : 5;  // Echoed back from associated command packet
                ReplyMessageType messageType : 3;  // RT_CommandResponse
            } cmd;

            // CID_* commands not explicitly listed below use just the header

            // Response to CID_ReadPage* commands
            struct __attribute__((packed,aligned(4))) ReadPage
            {
                CommandReplyHeader header;
                uint8_t data[28];
            } readPage;

            // Response to CID_StopMeasurement commands
            struct __attribute__((packed,aligned(4))) StopMeasurement
            {
                CommandReplyHeader header;
                uint32_t endTime;  // Measurement duration in microseconds (might wrap)
                uint64_t endOffset;  // Measurement data size in bytes
                uint32_t liveTxLost;  // 28-byte chunks dropped due to buffer overflow (radio link)
                uint32_t sdWriteLost;  // 28-byte chunks dropped due to buffer overflow (SD card)
            } stopMeasurement;
        } reply;

        // Sensor node => host notification channel packets
        union __attribute__((packed,aligned(4))) Notify
        {
            struct __attribute__((packed,aligned(2))) Header
            {
                uint8_t channel;  // RF::Address::Notify (0x7f)
                NotifyMessageId messageId : 8;
            } header;

            // NodeId request or assignment acknowledgement
            struct __attribute__((packed,aligned(4))) NodeId
            {
                Header header;  // NID_NodeId
                uint8_t reserved;
                uint8_t nodeId;  // Current NodeId (0 if none assigned)
                NodeIdentification ident;  // Globally unique node identification and type info
            } nodeId;
        } notify;

        // Host => sensor node notification channel packets
        union __attribute__((packed,aligned(4))) NotifyReply
        {
            Notify::Header header;  // RF::Address::NotifyReply (0x7f)
            
            // NodeId assignment request
            struct __attribute__((packed,aligned(4))) SetNodeId
            {
                Notify::Header header;  // NID_SetNodeId
                uint8_t reserved;
                uint8_t nodeId;  // NodeId to be assigned (0 unassigns and triggers re-request)
                HwUniqueId hwId;  // Globally unique node identification
                uint8_t reserved2[16];
            } setNodeId;
        } notifyReply;
    };

}
