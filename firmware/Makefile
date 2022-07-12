SITE_MK ?= ../site.mk

-include $(SITE_MK)

IVERILOG = iverilog
PYTHON = python3

AS = ${RISCV_TOOLCHAIN_PATH}${RISCV_TOOLCHAIN_PREFIX}as
OBJCOPY = ${RISCV_TOOLCHAIN_PATH}${RISCV_TOOLCHAIN_PREFIX}objcopy
OBJDUMP = ${RISCV_TOOLCHAIN_PATH}${RISCV_TOOLCHAIN_PREFIX}objdump
CC = ${RISCV_TOOLCHAIN_PATH}${RISCV_TOOLCHAIN_PREFIX}gcc

FIRMWARE_SOURCE = start.S ../lib/io.c ../lib/sd_card.c firmware.c

all: firmware.hex

$(SITE_MK):
	$(info Copy the example site.template file to site.mk and edit the paths.)
	$(error site.mk not found.)

firmware: firmware.hex

clean:
	rm -f *.hex *.elf *.bin *.lst

firmware.lst: firmware.elf
	${OBJDUMP} --disassemble firmware.elf > firmware.lst

firmware.hex: firmware.bin
	$(PYTHON) ../utils/makehex.py firmware.bin 4096 > firmware.hex

firmware.bin: firmware.elf firmware.lst
	${OBJCOPY} -O binary firmware.elf firmware.bin

firmware.elf: $(FIRMWARE_SOURCE) custom_ops.S firmware.ld
	${CC} -nostartfiles -nostdlib -Os -T firmware.ld -I ../lib $(FIRMWARE_SOURCE) -o firmware.elf

.PHONY: all clean firmware
