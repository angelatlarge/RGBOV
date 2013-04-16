#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define nop()  __asm__ __volatile__("nop")

#define LED_PORT			PORTD
#define LED_BIT				7
#define LED_DDR				DDRD

volatile uint8_t latchingFlag;

int main() {

	
	DDRB = 0xFF;
	DDRD = 0xFF;

	sei();				// interrupt enable

	LED_PORT |= 1<<LED_BIT;
	
	while (1) {
		latchingFlag=1;
		if (latchingFlag==0) {
			LED_PORT ^= 1<<LED_BIT;
			_delay_ms(100);
			latchingFlag = 1;
		}
	}
}