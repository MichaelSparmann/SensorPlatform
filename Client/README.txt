Setting up the SensorPlatform Client
====================================

(Tested on Ubuntu 16.04.1 LTS)

1. Enable the universe repository:
   $ sudo add-apt-repository universe
   $ sudo apt update

2. Install PyUSB 1.0:
   $ sudo apt install python3-usb

3. Set up non-root access to USB device:
   $ echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="f055", ATTR{idProduct}=="5053", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/50-sensorplatform.rules
   $ sudo udevadm control --reload-rules

4. Plug in the receiver device

5. Start the client:
   $ ./client.py rfsetup shell

6. The client should start and sensor nodes (if present) should connect.
   Basic information about available commands can be shown with the "help" command.

7. Shut down the radio interface to put sensor nodes to sleep
   (conserves a lot of battery power) and terminate the client:
   > stopradio
   > exit
