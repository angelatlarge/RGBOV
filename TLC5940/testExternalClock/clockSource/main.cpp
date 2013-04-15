#include <avr/io.h>
#include <avr/interrupt.h>
//~ #include <stdlib.h>
#include <util/delay.h>
//~ #include "serial.h"
//~ #include "strfmt.h"
#define VOLREG

#define nop()  __asm__ __volatile__("nop")

int main() {
	CLKPR = 1<<CLKPCE;
#if CLOCK_PRESCALE==1
#	warning Clock prescaler set to 1
	CLKPR = (0<<CLKPS3)|(0<<CLKPS2)|(0<<CLKPS1)|(0<<CLKPS0);
#elif CLOCK_PRESCALE==8
#	warning Clock prescaler set to 8
	CLKPR = (0<<CLKPS3)|(0<<CLKPS2)|(1<<CLKPS1)|(1<<CLKPS0);
#elif CLOCK_PRESCALE==16
#	warning Clock prescaler set to 16
	CLKPR = (0<<CLKPS3)|(1<<CLKPS2)|(0<<CLKPS1)|(0<<CLKPS0);
#elif CLOCK_PRESCALE==64
#	warning Clock prescaler set to 64
	CLKPR = (0<<CLKPS3)|(1<<CLKPS2)|(1<<CLKPS1)|(0<<CLKPS0);
#elif CLOCK_PRESCALE==128
#	warning Clock prescaler set to 128
	CLKPR = (0<<CLKPS3)|(1<<CLKPS2)|(1<<CLKPS1)|(1<<CLKPS0);
#elif CLOCK_PRESCALE==256
#	warning Clock prescaler set to 256
	CLKPR = (1<<CLKPS3)|(0<<CLKPS2)|(0<<CLKPS1)|(0<<CLKPS0);
#else
#error Clock prescaler not set (correctly)
#endif	
	
	// Set SPI pins to output
	DDRD |= 1<<5;
	
	// Timer0: OC0B=Pin 11 is free, using it for the greyscale clock
	TCCR0A = 0
		|(0<<COM0A1)	// Bits 7:6 – COM0A1:0: Compare Match Output A Mode
		|(0<<COM0A0)	// toggle
		|(1<<COM0B1)	// Bits 5:4 – COM0B1:0: Compare Match Output B Mode
		|(0<<COM0B0)	// output on OC0B = PD5
		|(1<<WGM01)		// Bits 1:0 – WGM01:0: Waveform Generation Mode
		|(1<<WGM00)
		;
	TCCR0B = 0
		|(0<<FOC0A)		// Force Output Compare A
		|(0<<FOC0B)		// Force Output Compare B
		|(1<<WGM02)		// Bit 3 – WGM02: Waveform Generation Mode
		|(0<<CS02)		// Bits 2:0 – CS02:0: Clock Select
		|(0<<CS01)
		|(1<<CS00)		// 010 = 8, 001 = 1
		;
	OCR0A = 1;
	OCR0B = 0;
	TIMSK0 = 0
		//~ |(1<<TOIE0);	// Timer/Counter0 Overflow Interrupt Enable
		;


	for (;;) {
	}
}