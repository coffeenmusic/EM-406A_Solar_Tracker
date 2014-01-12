GCCFLAGS=-g -Os -Wall -mmcu=atmega168 
LINKFLAGS=-Wl,-u,vfprintf -lprintf_flt -Wl,-u,vfscanf -lscanf_flt
AVRDUDEFLAGS=-c avr109 -p m168 -b 115200 -P COM3
LINKOBJECTS=../libnerdkits/delay.o ../libnerdkits/lcd.o ../libnerdkits/uart.o -lm

all:	solar_tracker-upload

solar_tracker.hex:	solar_tracker.c
	make -C ../libnerdkits
	avr-gcc ${GCCFLAGS} ${LINKFLAGS} -o solar_tracker.o solar_tracker.c ${LINKOBJECTS}
	avr-objcopy -j .text -O ihex solar_tracker.o solar_tracker.hex
	
solar_tracker.ass:	solar_tracker.hex
	avr-objdump -S -d solar_tracker.o > solar_tracker.ass
	
solar_tracker-upload:	solar_tracker.hex
	avrdude ${AVRDUDEFLAGS} -U flash:w:solar_tracker.hex:a
