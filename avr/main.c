/*
 * Firmware for TWILight
 *
 * Copyright (C) Josef Gajdusek <atx@atx.name>
 *
 * GitHub project:
 * https://github.com/atalax/TWILight
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * */


#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>

#define FW_VERSION		0x02
#define TWI_ADDRESS		0x32
#define TWI_GPIO_BASE	0x00
#define TWI_GPIO0		TWI_GPIO_BASE
#define TWI_GPIO1		TWI_GPIO_BASE + 1
#define TWI_GPIO2		TWI_GPIO_BASE + 2
#define TWI_GPIO3		TWI_GPIO_BASE + 3
#define TWI_GPIO4		TWI_GPIO_BASE + 4
#define TWI_GPIO5		TWI_GPIO_BASE + 5
#define TWI_GPIO6		TWI_GPIO_BASE + 6
#define TWI_GPIO7		TWI_GPIO_BASE + 7
#define TWI_GPIO8		TWI_GPIO_BASE + 8
#define TWI_GPIO9		TWI_GPIO_BASE + 9
#define TWI_LED0A		0x10
#define TWI_LED0B		0x11
#define TWI_LED2A		0x12
#define TWI_LED2B		0x13
#define TWI_MAGIC0		0x14
#define TWI_MAGIC1		0x15
#define TWI_MAGIC2		0x16
#define TWI_VER			0x17

#define TWS_RX_START	0x60
#define TWS_RX_ACK		0x80
#define TWS_RX_NACK		0x88
#define TWS_RX_END		0xa0

#define TWS_TX_START	0xa8
#define TWS_TX_ACK		0xb8
#define TWS_TX_NACK		0xc0
#define TWS_TX_END		0xc8

#define ASSEMBLE_GPIO(bank, bit) \
	(!!(DDR##bank & _BV(bit))) | \
	(!!(PORT##bank & _BV(bit)) << 1) | \
	(!!(PIN##bank & _BV(bit)) << 2)

#define DISASSEMBLE_GPIO(bank, bit, value) \
	if(!!(DDR##bank & _BV(bit)) != (value & _BV(0))) DDR##bank ^= _BV(bit); \
	if(!!(PORT##bank & _BV(bit)) != !!(value & _BV(1))) PORT##bank ^= _BV(bit);

int main() {
	TWAR = TWI_ADDRESS << 1;
	TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);

	TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM00) | _BV(WGM01);
	TCCR0B = _BV(CS00);

	TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM20) | _BV(WGM21);
	TCCR2B = _BV(CS20);

	DDRB = _BV(PIN3);
	DDRD = _BV(PIN3) | _BV(PIN5) | _BV(PIN6);

	sei();

	while(1) {}
}

uint8_t get_address(uint8_t addr) {
	switch(addr) {
	/* GPIO (00000<PIN><PORT><DDR>) */
	case TWI_GPIO0:
		return ASSEMBLE_GPIO(B, 2);
	case TWI_GPIO1:
		return ASSEMBLE_GPIO(B, 1);
	case TWI_GPIO2:
		return ASSEMBLE_GPIO(B, 0);
	case TWI_GPIO3:
		return ASSEMBLE_GPIO(D, 7);
	case TWI_GPIO4:
		return ASSEMBLE_GPIO(C, 1);
	case TWI_GPIO5:
		return ASSEMBLE_GPIO(C, 3);
	case TWI_GPIO6:
		return ASSEMBLE_GPIO(C, 2);
	case TWI_GPIO7:
		return ASSEMBLE_GPIO(D, 2);
	case TWI_GPIO8:
		return ASSEMBLE_GPIO(D, 4);
	case TWI_GPIO9:
		return ASSEMBLE_GPIO(C, 0);
	/* LEDs */
	case TWI_LED0B:
		return OCR0B;
	case TWI_LED0A:
		return OCR0A;
	case TWI_LED2A:
		return OCR2A;
	case TWI_LED2B:
		return OCR2B;
	/* Magic values */
	case TWI_MAGIC0:
		return 'A';
	case TWI_MAGIC1:
		return 'T';
	case TWI_MAGIC2:
		return 'X';
	case TWI_VER:
		return FW_VERSION;
	}
	return 0;
}


void set_address(uint8_t addr, uint8_t val) {
	switch(addr) {
	/* GPIO */
	case TWI_GPIO0:
		DISASSEMBLE_GPIO(B, 2, val);
		break;
	case TWI_GPIO1:
		DISASSEMBLE_GPIO(B, 1, val);
		break;
	case TWI_GPIO2:
		DISASSEMBLE_GPIO(B, 0, val);
		break;
	case TWI_GPIO3:
		DISASSEMBLE_GPIO(D, 7, val);
		break;
	case TWI_GPIO4:
		DISASSEMBLE_GPIO(C, 1, val);
		break;
	case TWI_GPIO5:
		DISASSEMBLE_GPIO(C, 3, val);
		break;
	case TWI_GPIO6:
		DISASSEMBLE_GPIO(C, 2, val);
		break;
	case TWI_GPIO7:
		DISASSEMBLE_GPIO(D, 2, val);
		break;
	case TWI_GPIO8:
		DISASSEMBLE_GPIO(D, 4, val);
		break;
	case TWI_GPIO9:
		DISASSEMBLE_GPIO(C, 0, val);
		break;
	/* LEDs */
	case TWI_LED0B:
		if(!val)
			TCCR0A &= ~(_BV(COM0B1));
		else
			TCCR0A |= _BV(COM0B1);
		OCR0B = val;
		break;
	case TWI_LED0A:
		if(!val)
			TCCR0A &= ~(_BV(COM0A1));
		else
			TCCR0A |= _BV(COM0A1);
		OCR0A = val;
		break;
	case TWI_LED2A:
		if(!val)
			TCCR2A &= ~(_BV(COM2A1));
		else
			TCCR2A |= _BV(COM2A1);
		OCR2A = val;
		break;
	case TWI_LED2B:
		if(!val)
			TCCR2A &= ~(_BV(COM2B1));
		else
			TCCR2A |= _BV(COM2B1);
		OCR2B = val;
		break;
	}
}

ISR(TWI_vect) {
	static uint8_t pointer = 0;
	static bool wrflag = false;

	switch(TWSR) {
	case TWS_RX_ACK:
		if(!wrflag) {
			pointer = TWDR;
			wrflag = true;
		}
		else {
			set_address(pointer, TWDR);
			pointer++;
		}
		break;
	case TWS_RX_END:
		wrflag = false;
		break;
	case TWS_TX_START:
	case TWS_TX_ACK:
		TWDR = get_address(pointer);
		pointer++;
	}
	TWCR |= _BV(TWEA);
}

