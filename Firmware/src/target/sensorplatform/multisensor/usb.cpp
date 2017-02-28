// Sensor node USB communication handling
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

// This is just a template for future work. It currently only enumerates and triggers
// WinUSB driver installation via WCID, but doesn't actually communicate with anything.
// See base station counterpart for reference how this could be implemented.


#include "global.h"
#include "usb.h"
#include "sys/util.h"
#include "interface/usb/wcid.h"
#include "../common/protocol/usbproto.h"


// Our only USB interface...
class __attribute__((aligned(4))) NodeInterface : public USB::Interface
{
public:
    // ...which has just one alternate setting...
    class __attribute__((aligned(4))) NodeAltsetting : public USB::AltSetting
    {
    private:
        // ...which has one data out (host => device) endpoint...
        class __attribute__((aligned(4))) DataOutEndpoint : public USB::Endpoint
        {
        public:
            constexpr DataOutEndpoint(int number)
                : Endpoint(USB::EndpointNumber(USB::Out, number)) {}

        private:
            int ctrlRequest(USB::USB* usb, USB::SetupPacket* request, const void** response)
            {
                return -1;  // We don't expect any control requests concerning this endpoint.
            }

            void xferComplete(USB::USB* usb, int bytesLeft)
            {
                // Handle incoming packets here
            }

            void setupReceived(USB::USB* usb, uint32_t offset)
            {
                // This is a bulk endpoint (which cannot receive SETUP packets)
            }
        };

        // ...and one data in (base station => host) endpoint.
        class __attribute__((aligned(4))) DataInEndpoint : public USB::Endpoint
        {
        public:
            constexpr DataInEndpoint(int number)
                : Endpoint(USB::EndpointNumber(USB::In, number)) {}

            int ctrlRequest(USB::USB* usb, USB::SetupPacket* request, const void** response)
            {
                return -1;  // We don't expect any control requests concerning this endpoint.
            }

            void xferComplete(USB::USB* usb, int bytesLeft)
            {
                // Handle TX completion of outgoing packets here
            }

            void setupReceived(USB::USB* usb, uint32_t offset)
            {
                // This is a bulk endpoint (which cannot receive SETUP packets)
            }
        };

    public:
        const USB::Endpoint* const endpoints[2];
        DataOutEndpoint outEp;
        DataInEndpoint inEp;

    private:
        int ctrlRequest(USB::USB* usb, USB::SetupPacket* request, const void** response)
        {
            return -1;  // We don't expect any control requests concerning this interface.
        }

        void set(USB::USB* usb)
        {
            // This interface alternate setting was selected, bring up endpoints.
            usb->configureEp(outEp.number, USB::Bulk, 64);
            usb->configureEp(inEp.number, USB::Bulk, 64);
        }

        void unset(USB::USB* usb)
        {
            // Clean up endpoint configuration upon interface deselection.
            usb->unconfigureEp(outEp.number);
            usb->unconfigureEp(inEp.number);
        }

    public:
        constexpr NodeAltsetting(int outEpNum, int inEpNum)
            : AltSetting(ARRAYLEN(endpoints)), endpoints{&outEp, &inEp}, outEp(outEpNum), inEp(inEpNum) {}
    };

    const USB::AltSetting* const altSettings[1];
    NodeAltsetting altSetting;

public:
    constexpr NodeInterface(int outEpNum, int inEpNum)
        : Interface(ARRAYLEN(altSettings)), altSettings{&altSetting}, altSetting(outEpNum, inEpNum) {}
};

enum InterfaceNumber
{
    SensorNodeInterface = 0,
};

enum OutEndpoint
{
    StandardControlOutEndpoint = 0,
    DataOutEndpoint,
    OutEndpointCount
};

enum InEndpoint
{
    StandardControlInEndpoint = 0,
    DataInEndpoint,
    InEndpointCount
};

// USB configuration descriptor
class __attribute__((aligned(4))) MainConfiguration : public USB::Configuration
{
    static constexpr const struct __attribute__((packed)) _config_descriptor
    {
        USB::Descriptor::ConfigurationDescriptor cfg;
        USB::Descriptor::InterfaceDescriptor if0;
        USB::Descriptor::EndpointDescriptor if0out;
        USB::Descriptor::EndpointDescriptor if0in;
    } descriptor
    {
        USB::Descriptor::ConfigurationDescriptor(sizeof(struct _config_descriptor), 1, 1, 0,
                                                 USB::Descriptor::ConfigAttributes(true, true, false), 100),
        USB::Descriptor::InterfaceDescriptor(SensorNodeInterface, 0, 2,
                                             USB::Descriptor::Class(0xff, 0x53, 0x00), 0),
        USB::Descriptor::EndpointDescriptor(USB::EndpointNumber(USB::Direction::Out, DataOutEndpoint),
                                            USB::EndpointAttributes(USB::EndpointType::Bulk,
                                                                    USB::EndpointAttributes::NoSync,
                                                                    USB::EndpointAttributes::Data), 64, 0),
        USB::Descriptor::EndpointDescriptor(USB::EndpointNumber(USB::Direction::In, DataInEndpoint),
                                            USB::EndpointAttributes(USB::EndpointType::Bulk,
                                                                    USB::EndpointAttributes::NoSync,
                                                                    USB::EndpointAttributes::Data), 64, 0),
    };

    const USB::Interface* const interfaces[1];
    NodeInterface nodeInterface;

    void busReset(USB::USB* usb)
    {
    }

    void set(USB::USB* usb)
    {
    }

    void unset(USB::USB* usb)
    {
    }

public:
    constexpr MainConfiguration()
        : Configuration(&descriptor.cfg, ARRAYLEN(interfaces)), interfaces{&nodeInterface},
          nodeInterface(DataOutEndpoint, DataInEndpoint) {}
};
constexpr const MainConfiguration::_config_descriptor MainConfiguration::descriptor;

static MainConfiguration mainConfig;

static USB::Configuration* const usbConfigs[] =
{
    &mainConfig,
};

// USB device descriptor
static const USB::Descriptor::DeviceDescriptor usbDevDesc(0x210, USB::Descriptor::Class(0, 0, 0), 64,
                                                          USB::VID, USB::PID, USB::VER, 1, 2, 3, ARRAYLEN(usbConfigs));

// Windows compatible ID descriptor (triggers automatic installation of WinUSB driver)
static constexpr const struct __attribute__((packed)) _wcid_descriptor
{
    USB::WCID::Descriptor::SetHeader header;
    USB::WCID::Descriptor::CompatibleID compatId;
} wcidSet
{
    USB::WCID::Descriptor::SetHeader(0x06030000, sizeof(_wcid_descriptor)),
    USB::WCID::Descriptor::CompatibleID(0x00004253554e4957, 0),
};

// USB binary object store descriptor (signals WCID capability)
static constexpr const struct __attribute__((packed)) _bos_descriptor
{
    USB::Descriptor::BOSDescriptor bos;
    USB::Descriptor::Capability::Platform wcidCap;
    USB::WCID::DescriptorSetInfo wcidInfo[1];
} usbBOSDesc
{
    USB::Descriptor::BOSDescriptor(sizeof(struct _bos_descriptor), 1),
    USB::Descriptor::Capability::Platform(USB_WCID_CAPABILITY_UUID, sizeof(_bos_descriptor::wcidInfo)),
    USB::WCID::DescriptorSetInfo(0x06030000, sizeof(wcidSet), 0xff, 0),
};

// Auto-generated USB serial number string based on STM32 chip hardware unique ID
static struct USBStrSerial
{
    ::USB::Descriptor::StringDescriptor header = ::USB::Descriptor::StringDescriptor(ARRAYLEN(string));
    char16_t string[24];

    USBStrSerial()
    {
        const uint32_t* src = STM32::CPUID;
        char16_t* dest = string;
        for (int i = 0; i < 3; i++)
        {
            uint32_t val = *src++;
            for (int j = 0; j < 8; j++)
            {
                *dest++ = hextab[val >> 28];
                val <<= 4;
            }
        }
    }
} usbStrSerial;

// USB string descriptors
static const USB::Descriptor::StringDescriptor* const usbStrDescs[] =
{
    &USB::STR_LANGUAGE.header,
    &USB::STR_VENDOR.header,
    &USB::STR_PRODUCT_NODE.header,
    &usbStrSerial.header,
};

// USB stack instance
STM32::USB usb(&usbDevDesc, &usbBOSDesc.bos, usbStrDescs, ARRAYLEN(usbStrDescs), usbConfigs, ARRAYLEN(usbConfigs));

// GET_DESCRIPTOR hook for WCID descriptor
static struct WCIDHook
{
    static int ctrlRequestHook(USB::USB* usb, USB::SetupPacket* request, const void** response)
    {
        if (request->bmRequestType.type != USB::BmRequestType::Vendor) return -1;
        if (request->bmRequestType.recipient == USB::BmRequestType::Device)
        {
            if (request->bRequest == 0xff && request->wIndex == USB::WCID::RequestCode::GetDescriptorSet)
            {
                if (!request->wLength || request->wValue) return -1;
                *response = &wcidSet;
                return MIN(request->wLength, wcidSet.header.wTotalLength);
            }
        }
        return -1;
    }

    WCIDHook()
    {
        usb.ctrlRequestHook = ctrlRequestHook;
    }
} wcidHook;
