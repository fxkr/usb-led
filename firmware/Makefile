PRG            = main
OBJ            = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o osccal.o ws2812b.o timer.o main.o
MCU_TARGET     = attiny85
OPTIMIZE       = -O2

DEFS           = -DF_CPU=16500000UL

CC             = avr-gcc

CFLAGS        = -std=c99 -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) -Iusbdrv -I. $(DEFS)
LDFLAGS       = -Wl,-Map,$(PRG).map

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump


.PHONY: all flash clean read-fuses write-fuses libusb show-size show-bss


all: libusb $(PRG).hex

clean:
	rm -f *.o $(PRG).elf $(PRG).hex $(PRG).bin
	rm -f *.lst *.map $(EXTRA_CLEAN_FILES)
	rm -f usbdrv/*.o usbdrv/oddebug.s usbdrv/usbdrv.s

flash: $(PRG).hex
	avrdude -p attiny85 -c usbasp -e -U flash:w:$(PRG).hex

read-fuses:
	avrdude -c usbasp -p attiny85 -U hfuse:r:/dev/null:h -U lfuse:r:/dev/null:h

write-fuses:
	avrdude -c usbasp -p attiny85 -U lfuse:w:0xf1:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m

show-size:
	avr-size --mcu=$(MCU_TARGET) --format=avr $(PRG).elf

show-bss:
	avr-objdump -t $(PRG).elf | grep "\.bss"


.c.o:
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

.S.o:
	$(CC) $(CFLAGS) $(LDFLAGS) -x assembler-with-cpp -c $< -o $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@
	avr-size --mcu=$(MCU_TARGET) --format=avr $<

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

lst: $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@


libusb: usbdrv/usbdrv.c

.PRECIOUS: usbdrv/usbdrv.c
usbdrv/usbdrv.c:
	wget -q https://github.com/obdev/v-usb/archive/releases/20121206.tar.gz -O- \
		| tar -xzf - --strip-components=1 v-usb-releases-20121206/usbdrv/

