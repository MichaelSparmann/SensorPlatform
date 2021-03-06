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
#include "interface/gpio/gpio.h"
#include "sys/util.h"
#include GPIO_STATIC_CONTROLLER_HEADER

#ifdef GPIO_DYNAMIC_CONTROLLERS
GPIO::PinController* gpio_controllers[GPIO_DYNAMIC_CONTROLLERS + 1] =
{
    &GPIO_STATIC_CONTROLLER
};
#define GPIO_CONTROLLER (*gpio_controllers[pin.controller])
#else
#define GPIO_CONTROLLER GPIO_STATIC_CONTROLLER
#endif

bool GPIO_OPTIMIZE GPIO::getLevel(Pin pin)
{
    return GPIO_CONTROLLER.getLevel(pin.pin);
}

void GPIO_OPTIMIZE GPIO::setLevel(Pin pin, bool level)
{
    GPIO_CONTROLLER.setLevel(pin.pin, level);
}

void GPIO_OPTIMIZE GPIO::setMode(Pin pin, enum mode mode)
{
    GPIO_CONTROLLER.setMode(pin.pin, mode);
}

void GPIO_OPTIMIZE GPIO::setType(Pin pin, enum type type)
{
    GPIO_CONTROLLER.setType(pin.pin, type);
}

void GPIO_OPTIMIZE GPIO::setPull(Pin pin, enum pull pull)
{
    GPIO_CONTROLLER.setPull(pin.pin, pull);
}

void GPIO_OPTIMIZE GPIO::setSpecial(Pin pin, int function)
{
    GPIO_CONTROLLER.setSpecial(pin.pin, function);
}

void GPIO_OPTIMIZE GPIO::configure(Pin pin, enum mode mode, bool level)
{
    GPIO_CONTROLLER.setLevel(pin.pin, level);
    GPIO_CONTROLLER.setMode(pin.pin, mode);
}

void GPIO_OPTIMIZE GPIO::configure(Pin pin, int function)
{
    GPIO_CONTROLLER.setSpecial(pin.pin, function);
    GPIO_CONTROLLER.setMode(pin.pin, GPIO::MODE_SPECIAL);
}

void GPIO_OPTIMIZE GPIO::configure(Pin pin, enum mode mode, bool level, enum type type, enum pull pull, int function)
{
    GPIO_CONTROLLER.setLevel(pin.pin, level);
    GPIO_CONTROLLER.setSpecial(pin.pin, function);
    GPIO_CONTROLLER.setType(pin.pin, type);
    GPIO_CONTROLLER.setPull(pin.pin, pull);
    GPIO_CONTROLLER.setMode(pin.pin, mode);
}

#ifdef GPIO_SUPPORT_FAST_MODE
bool GPIO_OPTIMIZE GPIO::enableFast(Pin pin, bool on)
{
    return GPIO_CONTROLLER.enableFast(pin.pin, on);
}

bool GPIO_OPTIMIZE GPIO::getLevelFast(Pin pin)
{
    return GPIO_CONTROLLER.getLevelFast(pin.pin);
}

void GPIO_OPTIMIZE GPIO::setLevelFast(Pin pin, bool level)
{
    GPIO_CONTROLLER.setLevelFast(pin.pin, level);
}
#else
bool GPIO_OPTIMIZE GPIO::enableFast(Pin pin, bool on)
{
    return false;
}

bool GPIO_OPTIMIZE GPIO::getLevelFast(Pin pin)
{
    return GPIO_CONTROLLER.getLevel(pin.pin);
}

void GPIO_OPTIMIZE GPIO::setLevelFast(Pin pin, bool level)
{
    GPIO_CONTROLLER.setLevel(pin.pin, level);
}
#endif
