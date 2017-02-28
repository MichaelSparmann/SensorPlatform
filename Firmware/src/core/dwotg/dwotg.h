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
#include "core/dwotg/regs.h"


// Synopsys DesignWare USB OTG hardware interface driver
class __attribute__((packed,aligned(4))) DWOTG : public USB::USB
{
    // Hardware register base address
    volatile DWOTGRegs::core_regs* regs __attribute__((aligned(4)));
protected:
    // Driver configuration
    bool phy16bit : 1;
    bool phyUlpi : 1;
    bool useDma : 1;
    bool sharedTxFifo : 1;
    bool disableDoubleBuffering : 1;
    uint32_t : 3;
    uint8_t fifoCount;
    uint16_t totalFifoSize;
    const uint16_t* fifoSizeList;
    // Main packet buffer
    Buffer buffer;
    // Endpoint state information
    struct __attribute__((packed,aligned(4))) EndpointDataPtr
    {
        uint32_t* rxAddr;
        const uint32_t* txAddr;
        constexpr EndpointDataPtr() : rxAddr(0), txAddr(0) {}
    } endpoints[0];
private:
    // SoC-specific low-level support functions
    virtual void socEnableClocks() = 0;
    virtual void socDisableClocks() = 0;
    virtual void socEnableIrq() = 0;
    virtual void socDisableIrq() = 0;
    virtual void socClearIrq() = 0;
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
    void flushOutEndpoint(int ep);
    void flushInEndpoint(int ep);
    void flushIrqs();
    void tryPush(int ep);
    void ep0Init();
public:
    void handleIrqInternal();
    // Driver constructor
    constexpr DWOTG(const ::USB::Descriptor::DeviceDescriptor* deviceDescriptor,
                    const ::USB::Descriptor::BOSDescriptor* bosDescriptor,
                    const ::USB::Descriptor::StringDescriptor* const* stringDescriptors, uint8_t stringDescriptorCount,
                    ::USB::Configuration* const* configurations, uint8_t configurationCount,
                    volatile DWOTGRegs::core_regs* regs, bool phy16bit, bool phyUlpi, bool useDma,
                    bool sharedTxFifo, bool disableDoubleBuffering,
                    uint8_t fifoCount, uint16_t totalFifoSize, const uint16_t* fifoSizeList)
        : USB(deviceDescriptor, bosDescriptor, stringDescriptors, stringDescriptorCount,
              configurations, configurationCount, &buffer, true),
          regs(regs), phy16bit(phy16bit), phyUlpi(phyUlpi), useDma(useDma), sharedTxFifo(sharedTxFifo),
          disableDoubleBuffering(disableDoubleBuffering), fifoCount(fifoCount), totalFifoSize(totalFifoSize),
          fifoSizeList(fifoSizeList), endpoints() {}
};

