Build instructions
==================

1. Add GNU ARM Embedded Toolchain PPA: (Xenial's ARM gcc 4.9 is buggy)
   $ sudo add-apt-repository ppa:team-gcc-arm-embedded/ppa
   $ sudo apt update

2. Install gcc-arm-none-eabi toolchain:
   $ sudo apt install gcc-arm-embedded

3. Build:
   $ make TYPE=release

Output files will be located in build/sensorplatform/*/release/*.{elf,bin}
The .bin file is a raw flash image for the microcontroller,
the .elf file can be used for debugging with e.g. gdb.
