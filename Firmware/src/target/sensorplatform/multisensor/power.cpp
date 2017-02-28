// Sensor node power management logic
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
#include "power.h"
#include "cpu/arm/cortexm/cmsis.h"
#include "soc/stm32/gpio.h"
#include "soc/stm32/gpio_regs.h"
#include "soc/stm32/pwr_regs.h"
#include "soc/stm32/f0/rcc.h"
#include "soc/stm32/f0/rcc_regs.h"
#include "interface/clockgate/clockgate.h"
#include "sys/util.h"
#include "sys/time.h"
#include "driver/clock.h"
#include "irq.h"
#include "radio.h"
#include "sd.h"
#include "sensortask.h"
#include "sensor/imu.h"


namespace Power
{
    // One-time init after system reset
    void init()
    {
        // Disable backup power domain write protection,
        // put internal MCU regulator into low-power mode during sleep
        Clock::onFromPri0(STM32_PWR_CLOCKGATE);
        union STM32_PWR_REG_TYPE::CR CR = { 0 };
        CR.b.DBP = true;
        CR.b.LPDS = true;
        STM32_PWR_REGS.CR.d32 = CR.d32;
        Clock::offFromPri0(STM32_PWR_CLOCKGATE);

        // Configure all pins of unused GPIO banks to be inputs
        for (int i = 2; i < 6; i++)
        {
            Clock::onFromPri0(STM32_GPIO_CLOCKGATE(i));
            STM32_GPIO_REGS(i).MODER.d32 = 0;
            Clock::offFromPri0(STM32_GPIO_CLOCKGATE(i));
        }

        // Configure (unused) RTC xtal oscillator pins
        GPIO::configure(PIN_C14, GPIO::MODE_ANALOG);
        GPIO::configure(PIN_C15, GPIO::MODE_ANALOG);
        
        // Power down microphone section, power up digital sensors (necessary for SPI bus access).
        // We're driving PMOS gates here, so true/false is inverted.
        GPIO::configure(PIN_PWR_MIC, GPIO::MODE_OUTPUT, true);
        GPIO::configure(PIN_PWR_SENSORS, GPIO::MODE_OUTPUT, false);

        // Power up digital sensors, configure communication pins accordingly.
        wakeSensors();
    }

    // Shut off power to digital sensors and SD card
    void sleepSensors()
    {
        // Pull I2C bus pins down instead of up to avoid current leakage through clamp diodes.
        // If we let them float, they pick up noise and cause current draw in the input buffers.
        GPIO::setPull(PIN_SENSORS_SCL, GPIO::PULL_DOWN);
        GPIO::setPull(PIN_SENSORS_SDA, GPIO::PULL_DOWN);
        // Same thing for the input pins of the SPI buses.
        GPIO::setPull(PIN_SD_MISO, GPIO::PULL_DOWN);
        GPIO::setPull(PIN_IMU_MISO, GPIO::PULL_DOWN);
        // Drive chip select pins of the SPI devices low to avoid clamp diode leakage.
        GPIO::setLevel(PIN_SD_NCS, false);
        GPIO::setLevel(PIN_IMU_NCS, false);
        // Shut off power to the digital sensors and SD card (inverted, we are driving a PMOS gate)
        GPIO::setLevel(PIN_PWR_SENSORS, true);
    }

    // Power up digital sensors and SD card
    void wakeSensors()
    {
        // Enable power to the digital sensors and SD card (inverted, we are driving a PMOS gate)
        GPIO::setLevel(PIN_PWR_SENSORS, false);
        // Drive chip select pins high to deselect the devices
        GPIO::setLevel(PIN_IMU_NCS, true);
        GPIO::setLevel(PIN_SD_NCS, true);
        // Pull SPI bus input pins high if no device drives them
        GPIO::setPull(PIN_IMU_MISO, GPIO::PULL_UP);
        GPIO::setPull(PIN_SD_MISO, GPIO::PULL_UP);
        // Pull I2C bus pins high (open-drain bus)
        GPIO::setPull(PIN_SENSORS_SCL, GPIO::PULL_UP);
        GPIO::setPull(PIN_SENSORS_SDA, GPIO::PULL_UP);
        // Notify IMU driver at which time we expect the IMU chip to be able to accept commands
        IMU::bootedAt = TIMEOUT_SETUP(20000);
        SensorTask::state = SensorTask::State_PowerUp;
    }

    // Power down microphone section
    void sleepMic()
    {
        GPIO::setLevel(PIN_PWR_MIC, true);  // inverted (PMOS gate)
    }

    // Power up microphone section
    void wakeMic()
    {
        GPIO::setLevel(PIN_PWR_MIC, false);  // inverted (PMOS gate)
    }

    // CPU deep sleep for a certain amount of time (in ~0.4ms units)
    void deepSleep(int delay)
    {
        // Clear pending wakeups
        Clock::onFromPri0(STM32_PWR_CLOCKGATE);
        union STM32_PWR_REG_TYPE::CR CR = { STM32_PWR_REGS.CR.d32 };
        CR.b.CWUF = true;
        STM32_PWR_REGS.CR.d32 = CR.d32;
        Clock::offFromPri0(STM32_PWR_CLOCKGATE);
        
        // Program wakeup in RTC
        Clock::setWakeupInterval(delay);
        
        // Enter deep sleep (switches us back to HSI oscillator as CPU clock source)
        SCB->SCR = SCB_SCR_SLEEPDEEP_Msk;
        idle();
        SCB->SCR = 0;
        
        // Re-enable HSI48 oscillator
        union STM32_RCC_REG_TYPE::CR2 CR2 = { STM32_RCC_REGS.CR2.d32 };
        CR2.b.HSI48ON = true;
        STM32_RCC_REGS.CR2.d32 = CR2.d32;
        
        // Switch clock source to HSI48 oscillator (will implicitly wait for it to start up)
        union STM32_RCC_REG_TYPE::CFGR CFGR = { 0 };
        CFGR.b.SW = CFGR.b.SW_HSI48;
        STM32_RCC_REGS.CFGR.d32 = CFGR.d32;
        
        // Wait for clock source switch to actually happen (otherwise HSI shutdown will be ignored)
        while (STM32_RCC_REGS.CFGR.b.SWS != CFGR.b.SWS_HSI48);
        
        // Shut down the now unused HSI oscillator to save power
        STM32_RCC_REGS.CR.b.HSION = false;
        
        // Disable RTC wakeup timer
        Clock::disableWakeup();
    }

    // Set this DPC pending to enter deep sleep once higher priority code returns
    void dpcSleepTask()
    {
        // Decide if we want to power down the digital sensors and SD card during sleep
        bool turnPowerOff = Radio::failedAssocAttempts >= 128;
        
        // Prepare SD card for sleep
        SD::shutdown(turnPowerOff);
        
        // IRQ lockout (will be kept locked until after wakeup)
        enter_critical_section();
        
        // Power down digital sensors and SD card if we decided to do that
        if (turnPowerOff) sleepSensors();
        
        // Deep sleep for 1.6s or 26s depending on the number of failed radio association attempts
        deepSleep(turnPowerOff ? 0xffff : 0xfff);
        
        // If the sensors and SD card were powered up before the sleep, we need to power them up.
        // (The radio SPI bus is shared with the sensors and may not be driven while the sensors
        // are powered down because of clamp diodes from the SPI pins to their supply rail)
        if (turnPowerOff)
        {
            Power::wakeSensors();
            IRQ::wakeSensorTask();
        }
        
        // Release IRQ lockout
        leave_critical_section();
        
        // Notify SD card driver that the card is powered again
        SD::wakeup(turnPowerOff);
        
        // Start scanning for radio base stations
        Radio::startup();
    }
}
