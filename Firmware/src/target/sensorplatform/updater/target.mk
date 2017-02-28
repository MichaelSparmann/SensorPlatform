NAME := updater
$(TARGET): build/$(TARGET)/$(TYPE)/$(NAME).bin
LISTINGS: build/$(TARGET)/$(TYPE)/$(NAME).elf.lst
LDSCRIPT := src/target/sensorplatform/updater/link.lds
_CFLAGS += -mcpu=cortex-m0 -mthumb
