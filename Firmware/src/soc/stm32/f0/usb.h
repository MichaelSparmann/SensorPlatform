#pragma once

// Generic Microcontroller Firmware Platform
// Copyright (C) 2017 Michael Sparmann
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
#include "soc/stm32/f0/usb_regs.h"


namespace STM32
{

    // STM32F0 USB hardware interface driver
    class __attribute__((packed,aligned(4))) USB : public ::USB::USB
    {
        // USB core interface
        void drvStart();
        void drvStop();
        void drvEp0StartRx(bool nonSetup, int len);
        void drvEp0StartTx(const void* buf, int len);
        int drvGetStall(::USB::EndpointNumber ep);
        void drvSetAddress(uint8_t address);
        void startRx(::USB::EndpointNumber ep, void* buf, int size);
        void startTx(::USB::EndpointNumber ep, const void* buf, int size);
        void setStall(::USB::EndpointNumber ep, bool stall);
        void configureEp(::USB::EndpointNumber ep, ::USB::EndpointType type, int maxPacket);
        void unconfigureEp(::USB::EndpointNumber ep);
        int getMaxTransferSize(::USB::EndpointNumber ep);
        // Internal helper functions
        void setRxSize(::USB::EndpointNumber ep, int size);
        void setAddress(uint8_t address);
        bool pushData(::USB::EndpointNumber ep);
        void copyToPMA(void* dest, const void* src, int len);
        void copyFromPMA(void* dest, const void* src, int len);
        void handleIrqInternal();
        // Main packet buffer
        Buffer buffer  __attribute__((aligned(4)));
        // Endpoint state information
        struct __attribute__((packed,aligned(4))) EndpointState
        {
            const void* txAddr;
            void* rxAddr;
            uint16_t txBytes;
            uint16_t rxBytes;
            constexpr EndpointState() : txAddr(NULL), rxAddr(NULL), txBytes(0), rxBytes(0) {}
        } epState[7];
        uint8_t pendingAddress;
        // Pointer to driver instance for IRQ handlers
        static USB* activeInstance;
    public:
        static void handleIrq();
        // Driver constructor
        constexpr USB(const ::USB::Descriptor::DeviceDescriptor* deviceDescriptor,
                      const ::USB::Descriptor::BOSDescriptor* bosDescriptor,
                      const ::USB::Descriptor::StringDescriptor* const* stringDescriptors,
                      uint8_t stringDescriptorCount, ::USB::Configuration* const* configurations,
                      uint8_t configurationCount)
            : ::USB::USB(deviceDescriptor, bosDescriptor, stringDescriptors, stringDescriptorCount, configurations,
                         configurationCount, &buffer, false), epState{}, pendingAddress(0) {}
    };

}
