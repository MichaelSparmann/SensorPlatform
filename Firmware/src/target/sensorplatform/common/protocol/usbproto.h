#pragma once

// SensorPlatform USB Protocol Definitions
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
#include "interface/usb/usb.h"
#include "sys/util.h"
#include "rfproto.h"


namespace USB
{
    // USB descriptor values
    constexpr const uint16_t VID = 0xf055;  // Not an officially assigned VID
    constexpr const uint16_t PID = 0x5053;
    constexpr const uint16_t VER = 0x0001;
    constexpr const USB_DECLARE_STRDESC(u"\x0409") STR_LANGUAGE;
    constexpr const USB_DECLARE_STRDESC(u"Michael Sparmann") STR_VENDOR;
    constexpr const USB_DECLARE_STRDESC(u"SensorPlatform Sensor Node") STR_PRODUCT_NODE;
    constexpr const USB_DECLARE_STRDESC(u"SensorPlatform Receiver") STR_PRODUCT_RECEIVER;

    // USB message / packet type IDs
    enum MessageType
    {
        CID_GetRadioStats = 0x0100,
        CID_StopRadio = 0x0200,
        CID_StartRadio = 0x0201,
        CID_PollDevice = 0x027e,
        CID_AssignSlots = 0x027f,
        CID_TransmitCommand = 0x0280,
        RID_CommandResult = 0x8001,
        NID_RFPacketReceived = 0xc001,
    };

    // Status codes for reply messages
    enum Status
    {
        Status_OK = 0x00000000,
        Status_UnknownError = 0x80000000,
        Status_UnknownCommand = 0x80000001,
        Status_InvalidArgument = 0x80000002,
        Status_HandlerFailed = 0x800000ff,
    };

    // Telemetry data structure
    struct __attribute__((packed,aligned(4))) RadioStats
    {
        uint32_t localTime;  // Local microsecond timer value
        uint32_t sofTotal;  // Total number of SOF packets sent
        uint32_t txTotal;  // Total number of non-SOF packets sent
        uint32_t rxAcked;  // Total number of RX packets acknowledged
        uint32_t rxSlotNotOwned;  // Total number of RX packets in slots where they didn't belong
        uint32_t rxOverflow;  // Total number of RX buffer overflow events (not packets)
        uint32_t reserved[8];
    };

    // USB packet format union
    union __attribute__((packed,aligned(4))) Packet
    {
        // Header structure that every USB packet starts with
        struct __attribute__((packed,aligned(2))) Header
        {
            MessageType type : 16;
            uint8_t seq;  // Sequence number of the command (will be copied into the response)
            uint8_t : 8;
        } header;

        // Command packets (Host => Receiver, unsolicited)
        union __attribute__((packed,aligned(4))) Command
        {
            // CID_* message types not explicitly listed below use just the header
            Header header;

            // Configures the radio chip and starts communication
            // (according to the specified channel attributes)
            struct __attribute__((packed,aligned(4))) StartRadio
            {
                Header header;  // CID_StartRadio
                RF::ExtendedChannelAttributes channel;
            } startRadio;

            // Assigns slots to NodeIDs. This can be sent with:
            //   CID_AssignSlots: Sets permanent assignment for a slot (set to 0 to clear)
            //   CID_PollDevice: Assign slots to the listed NodeIDs for just one frame
            //                   (Slots will be shuffled around, this is just a list of NodeIDs)
            struct __attribute__((packed,aligned(4))) AssignSlots
            {
                Header header;  // CID_PollDevice or CID_AssignSlots
                RF::TimeSlotOwner slot[ARRAYLEN(RF::Packet::SOF::slot)];
            } assignSlots;

            // Enqueues a packet for transmission
            struct __attribute__((packed,aligned(4))) TransmitCommand
            {
                Header header;  // CID_TransmitCommand
                uint8_t targetNode;
                uint8_t reserved[27];
                uint8_t command[32];
            } transmitCommand;
        } cmd;

        // Reply packets (Receiver => Host, in response to command packets)
        union __attribute__((packed,aligned(4))) Reply
        {
            Header header;  // seq will be echoed back from the command packet

            struct __attribute__((packed,aligned(4))) CommandResult
            {
                Header header;  // RID_CommandResult
                Status status : 32;
                union __attribute__((packed,aligned(4)))
                {
                    uint8_t u8[56];
                    uint32_t u32[14];
                    RadioStats getRadioStats;
                };
            } commandResult;
        } reply;

        // Notification packets (Receiver => Host, unsolicited)
        union __attribute__((packed,aligned(4))) Notify
        {
            Header header;

            // Received radio packet notification
            struct __attribute__((packed,aligned(4))) RFPacketReceived
            {
                Header header;  // NID_RFPacketReceived
                uint16_t sofCount;  // Low 16 bits of the current frame number
                uint16_t rxCount;  // Low 16 bits of the received (acknowledged) packet counter
                uint16_t index;  // Number of received radio packets transmitted via USB
                uint8_t reserved[22];
                uint8_t packet[32];
                // Note: sofCount/rxCount are captured at the time that the packet is processed
                //       by the USB layer and MAY NOT reflect the time that the packet is from!
            } rfPacketReceived;
        } notify;
    };

}
