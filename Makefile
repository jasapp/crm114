# Makefile loosely derived from generated avr toolchain makefile

PROG   := usbasp
MCU    := attiny25
TARGET := crm114

COMMONFLAGS = -g3 -O0 -funsigned-char -funsigned-bitfields \
  -fpack-struct -fshort-enums -Wall -mmcu=$(MCU) 

CFLAGS = -std=gnu99 $(COMMONFLAGS) -Wa,-adhlns=$(<:.c=.lst) -I/usr/local/include/simavr/avr/ -I/usr/local/include/simavr/ \

CXXFLAGS = -std=gnu++11 $(COMMONFLAGS) -fno-exceptions -fno-rtti \
  -Wa,-adhlns=$(<:.cc=.lst) \

ASFLAGS = -Wa,-adhlns=$(<:.S=.lst),-gstabs
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref

AVRDUDE:=avrdude -c $(PROG) -p t25
AVRDUDE_WRITE_FLASH = -u -Uflash:w:$(TARGET).hex:a -Ulfuse:w:0xd2:m -Uhfuse:w:0xde:m -Uefuse:w:0xff:m
AVRDUDE_READ_FLASH = -Uflash:r:flash.bin:r

TC = /usr

CC      = $(TC)/bin/avr-gcc
CXX     = $(TC)/bin/avr-c++
OBJCOPY = $(TC)/bin/avr-objcopy
OBJDUMP = $(TC)/bin/avr-objdump
SIZE    = $(TC)/bin/avr-size
SIMAVR  = /usr/local/bin/simavr 

default: $(TARGET).hex

sim: $(TARGET).elf
	$(SIMAVR) -g -t --mcu $(MCU) -f 8000000 $(TARGET).elf -v -v -v

check:
	$(AVRDUDE)

fuse:
	$(AVRDUDE) $(FUSE)

upload: clean default
	$(AVRDUDE) $(AVRDUDE_WRITE_FLASH)

upload_no_mem: CFLAGS += -DNO_MEM
upload_no_mem: upload

copy: 
	$(AVRDUDE) $(AVRDUDE_READ_FLASH)

run: 

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(SIZE) $@

.SECONDARY : $(TARGET).elf
.PRECIOUS : $(OBJ)

%.elf: %.o
	$(CC) $(CFLAGS) $< --output $@ $(LDFLAGS)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.s : %.c
	$(CC) -S $(CFLAGS) $< -o $@

%.o : %.S
	$(CC) -c $(ALL_ASFLAGS) $< -o $@

clean:
	-rm *.hex
	-rm *.lst
	-rm *.obj
	-rm *.elf
	-rm *.o
