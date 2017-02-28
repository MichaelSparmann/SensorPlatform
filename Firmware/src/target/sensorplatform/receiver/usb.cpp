// SensorPlatform Base Station USB Communication Handling
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
#include "usb.h"
#include "sys/util.h"
#include "soc/stm32/gpio.h"
#include "soc/stm32/f2/usb.h"
#include "interface/usb/wcid.h"
#include "irq.h"


namespace USB
{

    // Host => base station USB packet buffer (double buffered)
    Packet rxBuf[2];
    // Base station => host packet transmission queue (double buffered)
    Packet txBuf[2][USB_TX_BUFFERS];

    // Our only USB interface...
    class __attribute__((aligned(4))) ReceiverInterface : public Interface
    {
    public:
        // ...which has just one alternate setting...
        class __attribute__((aligned(4))) ReceiverAltsetting : public AltSetting
        {
        private:
            // ...which has one data out (host => base station) endpoint...
            class __attribute__((aligned(4))) DataOutEndpoint : public Endpoint
            {
                uint8_t nextBuf;  // Double buffering toggle
                uint8_t rxPending;  // Received data available for processing
                bool rxActive;  // USB RX transfer is pending
            public:
                bool connected;  // Whether the AltSetting is selected

            private:
                int ctrlRequest(USB* usb, SetupPacket* request, const void** response)
                {
                    return -1;  // We don't expect any control requests concerning this endpoint.
                }

                void xferComplete(USB* usb, int bytesLeft)
                {
                    // We have received a command packet.
                    // Mark the buffer as occupied and trigger command handling.
                    rxActive = false;
                    rxPending++;
                    IRQ::setPending(IRQ::DPC_HubHandlePackets);
                }

                void setupReceived(USB* usb, uint32_t offset)
                {
                    // This is a bulk endpoint (which cannot receive SETUP packets)
                }

            public:
                constexpr DataOutEndpoint(int number)
                    : Endpoint(EndpointNumber(Out, number)),
                      nextBuf(0), rxPending(0), rxActive(false), connected(false) {}

                    Packet* getPacket()
                    {
                        // If there a valid packet is available, return a pointer to it.
                        if (!rxPending) return NULL;
                        return &rxBuf[nextBuf];
                    }

                    void markProcessed(USB* usb)
                    {
                        // Mark the current packet as processed and start listening for
                        // new ones if necessary.
                        rxPending--;
                        nextBuf ^= 1;
                        tryStartRx(usb);
                    }

                    void tryStartRx(USB* usb)
                    {
                        // Check if we are connected, have a free buffer and aren't already active.
                        if (!connected || rxActive || rxPending > 1) return;
                        // If so, start receiving a command packet.
                        usb->startRx(number, &rxBuf[nextBuf ^ rxPending], sizeof(*rxBuf));
                        rxActive = true;
                    }
            };

            // ...and one data in (base station => host) endpoint.
            class __attribute__((aligned(4))) DataInEndpoint : public Endpoint
            {
                uint8_t nextBuf;  // Double buffering toggle
                uint8_t bufUsed;  // Number of used packets inside the next buffer
                bool txActive;  // Whether a buffer is currently being transmitted
                bool sendZLP;  // Whether we should send a Zero-Length Packet if a transmission
                               // finishes and we don't have anything else to send. This ensures
                               // that only one ZLP is sent in a row.
            public:
                bool connected;  // Whether the AltSetting is selected

            private:
                int ctrlRequest(USB* usb, SetupPacket* request, const void** response)
                {
                    return -1;  // We don't expect any control requests concerning this endpoint.
                }

                void xferComplete(USB* usb, int bytesLeft)
                {
                    // A transmission has finished.
                    GPIO::setLevelFast(PIN_LED1, false);
                    txActive = false;
                    // If we have anything left to send, start doing so.
                    if (!tryStartTx(usb) && sendZLP)
                    {
                        // If we don't, but we haven't sent a ZLP yet, do that.
                        // This causes early completion of a host-initiated multi-packet transfer,
                        // allowing the host to fetch all pending packets in one go without having
                        // to wait for a timeout (which is broken on Linux anyway).
                        sendZLP = false;
                        txActive = true;
                        usb->startTx(number, NULL, 0);
                    }
                    // As we have freed buffer space, trigger the command handler
                    // in case it was waiting for that.
                    IRQ::setPending(IRQ::DPC_HubHandlePackets);
                }

                void setupReceived(USB* usb, uint32_t offset)
                {
                    // This is a bulk endpoint (which cannot receive SETUP packets)
                }

            public:
                constexpr DataInEndpoint(int number)
                    : Endpoint(EndpointNumber(In, number)),
                      nextBuf(0), bufUsed(0), txActive(false), sendZLP(false), connected(false) {}

                Packet* getPacketBuf()
                {
                    // Returns a pointer to the next free transmission buffer if there is one
                    if (bufUsed >= ARRAYLEN(*txBuf)) return NULL;
                    return &txBuf[nextBuf][bufUsed];
                }

                void submitPacket()
                {
                    // Mark the next transmission buffer as used (and ready to be sent)
                    bufUsed++;
                }

                bool tryStartTx(USB* usb)
                {
                    // Attempt to start sending data if we are connected,
                    // not already sending and actually have data in the TX buffer.
                    if (!connected || txActive || !bufUsed) return false;
                    usb->startTx(number, txBuf[nextBuf], bufUsed * sizeof(**txBuf));
                    GPIO::setLevelFast(PIN_LED1, true);
                    txActive = true;
                    sendZLP = true;
                    nextBuf ^= 1;
                    bufUsed = 0;
                    return true;
                }
            };

        public:
            const Endpoint* const endpoints[2];
            DataOutEndpoint outEp;
            DataInEndpoint inEp;

        private:
            int ctrlRequest(USB* usb, SetupPacket* request, const void** response)
            {
                return -1;  // We don't expect any control requests concerning this interface.
            }

            void set(USB* usb)
            {
                // This interface alternate setting was selected, bring up endpoints.
                usb->configureEp(inEp.number, Bulk, 64);
                inEp.connected = true;
                inEp.tryStartTx(usb);
                usb->configureEp(outEp.number, Bulk, 64);
                outEp.connected = true;
                outEp.tryStartRx(usb);
            }

            void unset(USB* usb)
            {
                // Clean up endpoint configuration upon interface deselection.
                outEp.connected = false;
                usb->unconfigureEp(outEp.number);
                inEp.connected = false;
                usb->unconfigureEp(inEp.number);
            }

        public:
            constexpr ReceiverAltsetting(int outEpNum, int inEpNum)
                : AltSetting(ARRAYLEN(endpoints)), endpoints{&outEp, &inEp}, outEp(outEpNum), inEp(inEpNum) {}
        };

        const AltSetting* const altSettings[1];
        ReceiverAltsetting altSetting;

    public:
        constexpr ReceiverInterface(int outEpNum, int inEpNum)
            : Interface(ARRAYLEN(altSettings)), altSettings{&altSetting}, altSetting(outEpNum, inEpNum) {}
    };

    enum InterfaceNumber
    {
        SensorReceiverInterface = 0,
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
    class __attribute__((aligned(4))) MainConfiguration : public Configuration
    {
        static constexpr const struct __attribute__((packed)) _config_descriptor
        {
            Descriptor::ConfigurationDescriptor cfg;
            Descriptor::InterfaceDescriptor if0;
            Descriptor::EndpointDescriptor if0out;
            Descriptor::EndpointDescriptor if0in;
        } descriptor
        {
            Descriptor::ConfigurationDescriptor(sizeof(struct _config_descriptor), 1, 1, 0,
                                                Descriptor::ConfigAttributes(true, true, false), 100),
            Descriptor::InterfaceDescriptor(SensorReceiverInterface, 0, 2, Descriptor::Class(0xff, 0x52, 0x00), 0),
            Descriptor::EndpointDescriptor(EndpointNumber(Direction::Out, DataOutEndpoint),
                                           EndpointAttributes(EndpointType::Bulk, EndpointAttributes::NoSync,
                                                              EndpointAttributes::Data), 64, 0),
            Descriptor::EndpointDescriptor(EndpointNumber(Direction::In, DataInEndpoint),
                                           EndpointAttributes(EndpointType::Bulk, EndpointAttributes::NoSync,
                                                              EndpointAttributes::Data), 64, 0),
        };

    public:
        const Interface* const interfaces[1];
        ReceiverInterface receiverInterface;

    private:
        void busReset(USB* usb)
        {
        }

        void set(USB* usb)
        {
        }

        void unset(USB* usb)
        {
        }

    public:
        constexpr MainConfiguration()
            : Configuration(&descriptor.cfg, ARRAYLEN(interfaces)), interfaces{&receiverInterface},
              receiverInterface(DataOutEndpoint, DataInEndpoint) {}
    };
    constexpr const MainConfiguration::_config_descriptor MainConfiguration::descriptor;

    static MainConfiguration mainConfig;

    static Configuration* const usbConfigs[] =
    {
        &mainConfig,
    };

    // USB device descriptor
    static const Descriptor::DeviceDescriptor usbDevDesc(0x210, Descriptor::Class(0, 0, 0), 64,
                                                         VID, PID, VER, 1, 2, 3, ARRAYLEN(usbConfigs));

    // Windows compatible ID descriptor (triggers automatic installation of WinUSB driver)
    static constexpr const struct __attribute__((packed)) _wcid_descriptor
    {
        WCID::Descriptor::SetHeader header;
        WCID::Descriptor::CompatibleID compatId;
    } wcidSet
    {
        WCID::Descriptor::SetHeader(0x06030000, sizeof(_wcid_descriptor)),
        WCID::Descriptor::CompatibleID(0x00004253554e4957, 0),
    };

    // USB binary object store descriptor (signals WCID capability)
    static constexpr const struct __attribute__((packed)) _bos_descriptor
    {
        Descriptor::BOSDescriptor bos;
        Descriptor::Capability::Platform wcidCap;
        WCID::DescriptorSetInfo wcidInfo[1];
    } usbBOSDesc
    {
        Descriptor::BOSDescriptor(sizeof(struct _bos_descriptor), 1),
        Descriptor::Capability::Platform(USB_WCID_CAPABILITY_UUID, sizeof(_bos_descriptor::wcidInfo)),
        WCID::DescriptorSetInfo(0x06030000, sizeof(wcidSet), 0xff, 0),
    };

    // Auto-generated USB serial number string based on STM32 chip hardware unique ID
    static struct USBStrSerial
    {
        Descriptor::StringDescriptor header = Descriptor::StringDescriptor(ARRAYLEN(string));
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
    static const Descriptor::StringDescriptor* const usbStrDescs[] =
    {
        &STR_LANGUAGE.header,
        &STR_VENDOR.header,
        &STR_PRODUCT_RECEIVER.header,
        &usbStrSerial.header,
    };

    static const uint16_t fifoSizes[] = { 0x40, 0x60, 0x40 };

    // USB stack instance
    static STM32::USB usb(&usbDevDesc, &usbBOSDesc.bos, usbStrDescs, ARRAYLEN(usbStrDescs), usbConfigs,
                          ARRAYLEN(usbConfigs), STM32::OTG_FS, fifoSizes, ARRAYLEN(fifoSizes));

    // GET_DESCRIPTOR hook for WCID descriptor
    static struct WCIDHook
    {
        static int ctrlRequestHook(USB* usb, SetupPacket* request, const void** response)
        {
            if (request->bmRequestType.type != BmRequestType::Vendor) return -1;
            if (request->bmRequestType.recipient == BmRequestType::Device)
            {
                if (request->bRequest == 0xff && request->wIndex == WCID::RequestCode::GetDescriptorSet)
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

    // A bunch of convenience wrappers follow...

    void start()
    {
        usb.start();
    }

    void stop()
    {
        usb.stop();
    }

    bool isConnected()
    {
        return mainConfig.receiverInterface.altSetting.outEp.connected;
    }

    Packet* getPacket()
    {
        return mainConfig.receiverInterface.altSetting.outEp.getPacket();
    }

    void markProcessed()
    {
        mainConfig.receiverInterface.altSetting.outEp.markProcessed(&usb);
    }

    Packet* getPacketBuf()
    {
        return mainConfig.receiverInterface.altSetting.inEp.getPacketBuf();
    }

    void submitPacket()
    {
        mainConfig.receiverInterface.altSetting.inEp.submitPacket();
    }

    void tryStartTx()
    {
        mainConfig.receiverInterface.altSetting.inEp.tryStartTx(&usb);
    }

}
