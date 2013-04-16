#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define nop()  __asm__ __volatile__("nop")

#define LATCH_PORT			PORTD
#define LATCH_BIT			3
#define LATCH_DDR			DDRD

#define LED_PORT			PORTD
#define LED_BIT				7
#define LED_DDR				DDRD

volatile uint8_t latchingFlag;

ISR(TIMER2_OVF_vect) {
	if (latchingFlag>=1) {
		LATCH_PORT |=  (1<<LATCH_BIT);		// XLAT -> high
		//~ for (int i=0;i<1000;i++)
			//~ nop();
		LATCH_PORT &= ~(1<<LATCH_BIT);		// XLAT -> high
		//~ latchingFlag = 0;
	}
}

int main() {

	
	DDRB = 0xFF;
	DDRD = 0xFF;

	
/*
	// Timer0: OC0B=Pin 11 is free, using it for the greyscale clock
	TCCR0A = 0
		|(0<<COM0A1)	// Bits 7:6 – COM0A1:0: Compare Match Output A Mode
		|(0<<COM0A0)	// toggle
		|(1<<COM0B1)	// Bits 5:4 – COM0B1:0: Compare Match Output B Mode
		|(0<<COM0B0)
		|(1<<WGM01)		// Bits 1:0 – WGM01:0: Waveform Generation Mode
		|(1<<WGM00)
		;
	TCCR0B = 0
		|(0<<FOC0A)		// Force Output Compare A
		|(0<<FOC0B)		// Force Output Compare B
		|(1<<WGM02)		// Bit 3 – WGM02: Waveform Generation Mode
		|(0<<CS02)		// Bits 2:0 – CS02:0: Clock Select
		|(1<<CS01)
		|(0<<CS00)		// 010 = 8
		;
	OCR0A = 7;
	OCR0B = 3;
	TIMSK0 = 0
		//~ |(1<<TOIE0);	// Timer/Counter0 Overflow Interrupt Enable
		;
	// Clock / 64
*/
	// Timer2: Using OC2B for blank
	ASSR = 0;
	TCCR2A = 0
		|(0<<COM2A1)	// Bits 7:6 – COM0A1:0: Compare Match Output A Mode
		|(0<<COM2A0)	// 
		|(0<<COM2B1)	// Bits 5:4 – COM0B1:0: Compare Match Output B Mode
		|(0<<COM2B0)
		|(0<<WGM21)		// Bits 1:0 – WGM01:0: Waveform Generation Mode
		|(0<<WGM20)
		;
	TCCR2B = 0
		|(0<<FOC2A)		// Force Output Compare A
		|(0<<FOC2B)		// Force Output Compare B
		|(0<<WGM22)		// Bit 3 – WGM02: Waveform Generation Mode
		|(1<<CS22)		// Bits 2:0 – CS02:0: Clock Select
		|(1<<CS21)
		|(1<<CS20)		// 100 = 64
		;
	OCR2A = 255;
	OCR2B = 255;
	TIMSK2 = 0
		|(1<<TOIE2);	// Timer/Counter0 Overflow Interrupt Enable

	sei();				// interrupt enable
	

	LED_PORT |= 1<<LED_BIT;
	
	while (1) {
		latchingFlag=1;
		if (latchingFlag==0) {
			LED_PORT ^= 1<<LED_BIT;
			_delay_ms(100);
			latchingFlag = 1;
		}
		//~ for (int i=0;i<10000;i++)
			//~ nop();
	}
}