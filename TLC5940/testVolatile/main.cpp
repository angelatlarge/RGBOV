#include <avr/io.h>
#include <util/delay.h>

#define LED_PORT			PORTD
#define LED_BIT				7
#define LED_DDR				DDRD

volatile uint8_t latchingFlag;

int main() {
	
	LED_PORT &= ~(1<<LED_BIT);
	//~ LED_PORT |= (1<<LED_BIT);
	DDRD = 0xFF;
	for (;;) {
		latchingFlag=1;
		if (latchingFlag==0) {
			LED_PORT ^= 1<<LED_BIT;	// Toggle the LED
			//~ LED_PORT &= 1<<LED_BIT;	// Toggle the LED
			//~ PORTB |= 1<<7;
			_delay_ms(100);
			latchingFlag = 1;
		}
	}
}