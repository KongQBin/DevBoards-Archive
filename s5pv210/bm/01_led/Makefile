CROSS_COMPILE = arm-none-eabi-

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
READELF = $(CROSS_COMPILE)readelf


# 默认目标
all: usb


# USB Boot
start.o: start.S
	$(CC) -c -o start.o start.S


led_usb.elf: start.o usb_link.lds
	$(LD) -T usb_link.lds -o led_usb.elf start.o


led_usb.bin: led_usb.elf
	$(OBJCOPY) -O binary led_usb.elf led_usb.bin


usbtool: usbtool.c
	gcc usbtool.c -o usbtool -lusb-1.0


usb: led_usb.bin usbtool
	@echo
	@echo "USB image:"
	@ls -l led_usb.bin
	@echo
	@echo "Entry point:"
	@$(READELF) -h led_usb.elf | grep "Entry point"
	@echo
	@echo "Run:"
	@echo "  sudo ./usbtool led_usb.bin"


# 查看反汇编
dump:
	$(OBJDUMP) -d led_usb.elf


# SD Boot
led.bin: start.o link.lds
	$(LD) -T link.lds -o led.elf start.o
	$(OBJCOPY) -O binary led.elf led.bin


210.bin: led.bin sig
	./sig led.bin 210.bin


# SD 卡烧录
SD_DEV = /dev/sdb

burn: 210.bin
	sudo dd if=210.bin of=$(SD_DEV) seek=1 bs=512
	sync


# Clean
clean:
	rm -f \
	start.o \
	led.elf \
	led.bin \
	210.bin \
	led_usb.elf \
	led_usb.bin \
	usbtool
