#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>

#define FW_VERSION		0x01
#define TWI_ADDRESS		0x32

#define TWS_RX_START	0x60
#define TWS_RX_ACK		0x80
#define TWS_RX_NACK		0x88
#define TWS_RX_END		0xa0

#define TWS_TX_START	0xa8
#define TWS_TX_ACK		0xb8
#define TWS_TX_NACK		0xc0
#define TWS_TX_END		0xc8

#define GPIO_REG_NAME(a, r) a <= 0x01 ? &DDR##r : (a <= 0x03) ? &PIN##r : &PORT##r

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

	while(1) {
		//printf("%02x\n\r", TWSR);
	}
}

uint8_t get_address(uint8_t addr) {
	if(addr <= 0x05) { /* GPIO */
		volatile uint8_t *regb = GPIO_REG_NAME(addr, B);
		volatile uint8_t *regc = GPIO_REG_NAME(addr, C);
		volatile uint8_t *regd = GPIO_REG_NAME(addr, D);
		if(addr & 1) { /* G7 - G0 */
			return (!!(*regb & _BV(0)) << 2) | (!!(*regb & _BV(1)) << 1) | (!!(*regb & _BV(2)))
					| (!!(*regc & _BV(1)) << 4) | (!!(*regc & _BV(2)) << 6) | (!!(*regc & _BV(3)) << 5)
					| (!!(*regd & _BV(2)) << 7) | (!!(*regd & _BV(7)) << 3);
		}
		else {
			return ((*regc & 1) << 1) | !!(*regd & _BV(4));
		}
	}
	else {
		switch(addr) {
		case 0x06:
			return OCR0B;
		case 0x07:
			return OCR0A;
		case 0x08:
			return OCR2A;
		case 0x09:
			return OCR2B;
		case 0x10: /* Magic values */
			return 'A';
		case 0x11:
			return 'T';
		case 0x12:
			return 'X';
		case 0x13:
			return FW_VERSION;
		}
	}
	return 0;
}


void set_address(uint8_t addr, uint8_t val) {
	if(addr <= 0x05) { /* GPIO */
		volatile uint8_t *regb = GPIO_REG_NAME(addr, B);
		volatile uint8_t *regc = GPIO_REG_NAME(addr, C);
		volatile uint8_t *regd = GPIO_REG_NAME(addr, D);
		if(addr & 1) { /* G7 - G0 */
			*regb = (*regb & ~0b00000111) | (!!(val & _BV(2))) | (!!(val & _BV(1)) << 1) | (!!(val & _BV(0)) << 2);
			*regc = (*regc & ~0b00001110) | (!!(val & _BV(4)) << 1) | (!!(val & _BV(6)) << 2) | (!!(val & _BV(5)) << 3);
			*regd = (*regd & ~0b10000100) | (!!(val & _BV(7)) << 2) | (!!(val & _BV(3)) << 7);
		}
		else { /* G9, G8 */
			*regc = (*regc & ~1) | !!(val & 2);
			*regd = (*regd & ~_BV(4)) | ((val & 1) << 4);
		}
	}
	else { /* LEDs */
		switch(addr) {
		case 0x06: /* 0B */
			OCR0B = val;
			break;
		case 0x07: /* 0A*/
			OCR0A = val;
			break;
		case 0x08: /* 2A */
			OCR2A = val;
			break;
		case 0x09: /* 2B */
			OCR2B = val;
			break;
		}
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

