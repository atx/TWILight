NAME	= twilight
MCU	= atmega328p
F_CPU	= 12000000
FORMAT	= ihex
TARGET	= $(NAME)

AVRDUDE = avrdude -p$(MCU) -cusbasp -Uflash:w:$(NAME).hex:a

SRC	= main.c


CFLAGS	= -DF_CPU=$(F_CPU)UL
CFLAGS	+= -O3
CFLAGS	+= -Wall
CFLAGS	+= -std=gnu99
CFLAGS	+= -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -funsigned-char -funsigned-bitfields
CFLAGS	+= -mmcu=$(MCU)

LDFLAGS       = -Wl,-Map,$(PRG).map

CROSS	= avr-
SHELL	= sh
CC	= $(CROSS)gcc
OBJCOPY	= $(CROSS)objcopy
OBJDUMP	= $(CROSS)objdump
SIZE	= $(CROSS)size
NM	= $(CROSS)nm
RM	= rm -f


OBJ	= $(SRC:.c=.o)
LST	= $(SRC:.c=.lst)

all: $(NAME).elf hex

$(NAME).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

text: hex bin srec

hex:	$(NAME).hex

avrdude: hex
	$(AVRDUDE)

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

clean:
	rm -rf *.o *.elf *.hex
