#pragma once

// SensorPlatform Sensor Node
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


#define STM32_GPIO_CONFIG \
{ \
    GPIO_CONFIG_PORT( /* PORTA */ \
        /* PA 0 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_USART4_PA0, /* UART4_TX (EXT) */ \
        /* PA 1 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_USART4_PA1, /* UART4_RX (EXT) */ \
        /* PA 2 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_USART2_PA2, /* USART2_TX (EXT) */ \
        /* PA 3 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_USART2_PA3, /* USART2_RX (EXT) */ \
        /* PA 4 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_NSS (EXT) */ \
        /* PA 5 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_SCK (EXT) */ \
        /* PA 6 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_MISO (EXT) */ \
        /* PA 7 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_MOSI (EXT) */ \
        /* PA 8 */ MODE_OUTPUT, TYPE_PUSHPULL, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_TIM1_PA8, /* LED */ \
        /* PA 9 */ MODE_OUTPUT, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* NRF_CE */ \
        /* PA10 */ MODE_OUTPUT, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_HIGH, SPECIAL_SYS, /* NRF_nCS */ \
        /* PA11 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_USB, /* USB_DM */ \
        /* PA12 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_USB, /* USB_DP */ \
        /* PA13 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_50MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* SWDIO */ \
        /* PA14 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_50MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* SWCLK */ \
        /* PA15 */ MODE_OUTPUT, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* SD_nCS */ \
    ), \
    GPIO_CONFIG_PORT( /* PORTB */ \
        /* PB 0 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* ADC_IN8 (Mic, IChg) */ \
        /* PB 1 */ MODE_ANALOG, TYPE_PUSHPULL, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* ADC_IN9 (BattV) */ \
        /* PB 2 */ MODE_INPUT, TYPE_OPENDRAIN, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* BattChgState */ \
        /* PB 3 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_SCK (SD) */ \
        /* PB 4 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_DOWN, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_MISO (SD) */ \
        /* PB 5 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI1, /* SPI1_MOSI (SD) */ \
        /* PB 6 */ MODE_SPECIAL, TYPE_OPENDRAIN, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_HIGH, SPECIAL_I2C1_PB6, /* I2C1_SCL (Main) */ \
        /* PB 7 */ MODE_SPECIAL, TYPE_OPENDRAIN, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_HIGH, SPECIAL_I2C1_PB7, /* I2C1_SDA (Main) */ \
        /* PB 8 */ MODE_INPUT, TYPE_PUSHPULL, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* USERBTN */ \
        /* PB 9 */ MODE_OUTPUT, TYPE_PUSHPULL, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_HIGH, SPECIAL_TIM17_PB9, /* nEXT_ON */ \
        /* PB10 */ MODE_SPECIAL, TYPE_OPENDRAIN, SPEED_2MHZ, PULL_UP, INITCTL_SET, INITVAL_HIGH, SPECIAL_I2C2, /* I2C2_SCL (EXT) */ \
        /* PB11 */ MODE_SPECIAL, TYPE_OPENDRAIN, SPEED_2MHZ, PULL_UP, INITCTL_SET, INITVAL_HIGH, SPECIAL_I2C2, /* I2C2_SDA (EXT) */ \
        /* PB12 */ MODE_OUTPUT, TYPE_PUSHPULL, SPEED_2MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SYS, /* IMU_nCS */ \
        /* PB13 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI2_PB13, /* SPI2_SCK (Main) */ \
        /* PB14 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_DOWN, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI2_PB14, /* SPI2_MISO (Main) */ \
        /* PB15 */ MODE_SPECIAL, TYPE_PUSHPULL, SPEED_25MHZ, PULL_NONE, INITCTL_SET, INITVAL_LOW, SPECIAL_SPI2_PB15, /* SPI2_MOSI (Main) */ \
    ), \
}

#define RADIO_SPI_PRESCALER 2
#define RADIO_SPI_BUS STM32_SPI2_REGS
#define RADIO_SPI_CLK STM32_SPI2_CLOCKGATE
#define RADIO_TIMER STM32_TIM7_REGS
#define RADIO_TIMER_CLK STM32_TIM7_CLOCKGATE
#define RADIO_DMA_RX_CONTROLLER 0
#define RADIO_DMA_RX_STREAM 3
#define RADIO_DMA_RX_PRIORITY 3
#define RADIO_DMA_TX_CONTROLLER 0
#define RADIO_DMA_TX_STREAM 4
#define RADIO_DMA_TX_PRIORITY 2

#define PIN_RADIO_NCS PIN_A10
#define PIN_RADIO_CE PIN_A9
#define PIN_RADIO_NIRQ PIN_C13

#define SD_SPI_PRESCALER 0
#define SD_SPI_DISCOVERY_PRESCALER 6
#define SD_SPI_BUS STM32_SPI1_REGS
#define SD_SPI_CLK STM32_SPI1_CLOCKGATE
#define SD_DMA_RX_CONTROLLER 0
#define SD_DMA_RX_STREAM 1
#define SD_DMA_RX_PRIORITY 1
#define SD_DMA_TX_CONTROLLER 0
#define SD_DMA_TX_STREAM 2
#define SD_DMA_TX_PRIORITY 1
#define PIN_SD_NCS PIN_A15
#define PIN_SD_MISO PIN_B4
#define PIN_SD_MOSI PIN_B5

#define IMU_SPI_PRESCALER 5
#define IMU_SPI_PRESCALER_FAST 1
#define PIN_IMU_NCS PIN_B12
#define PIN_IMU_MISO PIN_B14

#define BARO_I2C_BUS I2CBus::I2C1
#define HYGRO_I2C_BUS I2CBus::I2C1
#define LIGHT_I2C_BUS I2CBus::I2C1

#define PIN_PWR_SENSORS PIN_F0
#define PIN_PWR_MIC PIN_F1
#define PIN_LED PIN_A8
#define PIN_SENSORS_SCL PIN_B6
#define PIN_SENSORS_SDA PIN_B7

// Debug LCD display (only on breadboard)
//#define PIN_LCD_CD PIN_B2
//#define PIN_LCD_CE PIN_B12
//#define LCD_SPI_PRESCALER 3
//#define LCD_TIMER STM32_TIM6_REGS
//#define LCD_TIMER_CLK STM32_TIM6_CLOCKGATE
//#define LCD_BIAS 3
//#define LCD_TC 2
//#define LCD_VOP 68

#define SENSORTASK_VECTOR "tim2_irqhandler"
#define STORAGETASK_VECTOR "dma1_stream2_3_dma2_stream1_2_irqhandler"

#define TICK_TIMER STM32_TIM2_REGS
#define TICK_TIMER_CLK STM32_TIM2_CLOCKGATE
#define TICK_TIMER_FREQ STM32_APB1_CLOCK

// Firmware version number to announce in node identification
// (increment this after any significant change)
#define FIRMWARE_VERSION 2

// Tunables:
// Command reception buffers (uses 32 * N bytes of RAM)
#define RADIO_RX_BUFFERS 2
// Data/response transmissions buffers (uses 32 * N bytes of RAM)
#define RADIO_TX_BUFFERS 32 // 64
// How many TX buffers should be reserved for responses
#define RADIO_TX_BUFFER_RESERVE 0
// Maximum number of attempts to transmit a command response
// (ignored, currently forced to unlimited in code)
#define TX_ATTEMPTS_RESPONSE 15
// Maximum number of attempts to transmit a measurement data packet
// (ignored, currently forced to unlimited in code)
#define TX_ATTEMPTS_DATA 255
// Attempt to request a NodeId every N usec if we don't have one
#define IDENT_ATTEMPT_INTERVAL 200000
// Drop NodeId if it didn't get any time slots for N usec
#define NODE_ID_TIMEOUT 3000000
// Number of measurement data buffers (must be at least 16, uses 476 * N bytes of RAM)
#define MAINBUF_BLOCK_COUNT 24
// (Main) stack size in bytes. The other stacks are configured in sensortask.cpp and storagetask.cpp.
//#define STACK_SIZE 1024

#define GPIO_SUPPORT_FAST_MODE
#define CORTEXM_DISABLE_SYSTICK
//#define STM32_USE_INOFFICIAL
#define STM32_ENABLE_EXTI
#define STM32_ENABLE_USB
#define STM32_ENABLE_CRC32
#include "board/sensorplatform/multisensor/target.h"
