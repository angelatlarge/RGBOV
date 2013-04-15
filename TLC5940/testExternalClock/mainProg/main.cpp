#include <avr/io.h>
#include <avr/interrupt.h>
//~ #include <stdlib.h>
#include <util/delay.h>
//~ #include "serial.h"
//~ #include "strfmt.h"
#define VOLREG

#define nop()  __asm__ __volatile__("nop")

#define TLC5940_XLAT_BIT	7
#define TLC5940_XLAT_PORT	PORTD
#define TLC5940_XLAT_DDR	DDRD
#define TLC5940_XLAT_PINS	PIND

#define TLC5940_BLANK_BIT	3
#define TLC5940_BLANK_PORT	PORTD
#define TLC5940_BLANK_DDR	DDRD
#define TLC5940_BLANK_PINS	PIND

register uint8_t latchingFlag asm("r2");

void sendSerial() {
	
	uint32_t nBitsSent = 0;
	
	/* 	Padding: we need to pre-pad
		We will send 15 12-bit values (180 bits) which is 22.5 8-bit SPI 8-bit bytes
		For every TLC5490 we actually need to send 16 12-bit values (192 bits in total)
		because that's what the TLC5490 serial comms expect. 
		Since we are using only 15 of the output lines, 
		the 16th value is unnecessary (we are not using OUT15)
		We also need to pad for the extra nibble (22.5 SPI above) 
		So if we start by sending the empty 12-bit value, we are OK
		This padding needs to be done for every chip
	*/
	//~ padSPI(0);
	SPDR = 0;					// First 8 bits of the empty 12-bit data
	while(!(SPSR & (1<<SPIF))) {
		// Wait for the transfer to finish
	}
	SPDR = 0;					// Last 4 bits of the empty 12-bit data
								// plus the empty 4 high bits of the first intensity data
	while(!(SPSR & (1<<SPIF))) {
		// Wait for the transfer to finish
	}
	nBitsSent += 16;
	VOLREG uint8_t idxNextSendType = 2;	// The next send type is type 2
										// so we indicate that
	
	// Different curColDatas are not implemented yet
	//~ uint8_t * curColData = nColumnData[idxUnit];
	
	VOLREG uint8_t idxChannel = 0;
	VOLREG uint8_t nPixelsSent = 0;
	VOLREG uint8_t dataByte = (idxChannel==0)?0xFF:0x00;
	VOLREG uint8_t toSPDR = dataByte;
	
	for (;;) { // Send bytes loop
		SPDR = toSPDR;			// Send the byte
		nBitsSent += 8;

		while(!(SPSR & (1<<SPIF))) {
			// Wait for the transfer to finish
		}
#define PADDING
#ifdef PADDING
		if (nBitsSent % 192 == 0) {
			if (nBitsSent >= 192)
				break;
			
			
			SPDR = 0;					// First 8 bits of the empty 12-bit data
			while(!(SPSR & (1<<SPIF))) {
				// Wait for the transfer to finish
			}
			SPDR = 0;					// Last 4 bits of the empty 12-bit data
										// plus the empty 4 high bits of the first intensity data
			while(!(SPSR & (1<<SPIF))) {
				// Wait for the transfer to finish
			}
			idxNextSendType = 1;		// It will get incremented first
										// so the first byte will actually be
										// idxNextSendType = 2

			nBitsSent += 16;
		}
#endif								
		
		// Update the send type
		if (++idxNextSendType>2) { idxNextSendType = 0; }
		//~ ++idxNextSendType %= 4;
		
		// Prepare the next one for loading
		if (idxNextSendType==1) {
				// Case 1: Need to send the low nibble + padding
				toSPDR = dataByte<<4;
		} else {
			// Either way we need another new data in dataByte
			// 
			if (idxChannel==0) {
				// Load next palette index
				nPixelsSent++;
			}
			dataByte = (idxChannel==0)?0xFF:0x00;
			
			if (++idxChannel>2) idxChannel = 0;
			//~ This fails: ++idxChannel %= 3;
			
			if (idxNextSendType) {
				// idxSendType == 2
				// Case 2: Need to send pure data
				toSPDR = dataByte;
			} else {
				// idxSendType == 0
				// Case 0: Need to send padding + high nibble
				toSPDR = dataByte >> 4;
			}
		}

		while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
			;				
		
	} // End of send bytes loop
		
		
	
//~ #define DEBUG_OUT	
#ifdef DEBUG_OUT
	//~ dputsi("VERTICAL_PIXELS: ", VERTICAL_PIXELS, 0);
	if (nBitsSent > 0) {
		dputs(" Bits sent: ", 0);
		dputi(nBitsSent, 0);
		dputs(" sent ", 1);
	}
#endif /* DEBUG_OUT */

}


ISR(TIMER2_OVF_vect) {
	if (latchingFlag>=1) {
		TLC5940_XLAT_PORT |=  (1<<TLC5940_XLAT_BIT);		// XLAT -> high
		//~ for (int i=0;i<10;i++)
			nop();
		TLC5940_XLAT_PORT &= ~(1<<TLC5940_XLAT_BIT);		// XLAT -> high
		latchingFlag = 0;
	}
	// Blank
	TLC5940_BLANK_PORT |= (1<<TLC5940_BLANK_BIT);
	//~ TLC5940_XLAT_PORT |=  (1<<TLC5940_XLAT_BIT);		// XLAT -> high
	//~ for (int i=0;i<10;i++)
		nop();
	//~ TLC5940_XLAT_PORT &= ~(1<<TLC5940_XLAT_BIT);		// XLAT -> high
	TLC5940_BLANK_PORT &= ~(1<<TLC5940_BLANK_BIT);
}

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
	DDRB = 0xFF;

	
	// Set up SPI
	SPCR = 0
		|(0<<SPIE)			// Interrupt enable
		|(1<<SPE)			// Bit 6 – SPE: SPI Enable
		|(0<<DORD)			// Bit 5 – DORD: Data Order
							// When the DORD bit is written to zero, the MSB of the data word is transmitted first
		|(1<<MSTR)			// Bit 4 – MSTR: Master/Slave Select
							// This bit selects Master SPI mode when written to one
		|(0<<CPOL)			// Bit 3 – CPOL: Clock Polarity
							// When CPOL is written to zero, SCK is low when idle.
		|(0<<CPHA)			// Bit 2 – CPHA: Clock Phase
		;
		
	//~ SPSR |= (0<<SPI2X); SPCR |= (1<<SPR1)|(0<<SPR0);		// 010 = Focs/64
	//~ SPSR |= (0<<SPI2X); SPCR |= (1<<SPR1)|(1<<SPR0);		// 010 = Focs/128
	SPSR |= (0<<SPI2X); SPCR |= (0<<SPR1)|(1<<SPR0);		// 010 = Focs/16
		;
	SPSR = 0
		//~ |(0<<SPI2X)			// Bit 0 – SPI2X: Double SPI Speed Bit
		|(0<<SPI2X)			// Bit 0 – SPI2X: Double SPI Speed Bit
		;



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
		|(0<<CS21)
		|(0<<CS20)		// 100 = 64
		;
	OCR2A = 255;
	OCR2B = 255;
	TIMSK2 = 0
		|(1<<TOIE2);	// Timer/Counter0 Overflow Interrupt Enable

	sei();				// interrupt enable
	
	DDRD |= (1<<5);
	TLC5940_BLANK_DDR |= (1<<TLC5940_BLANK_BIT);
	TLC5940_XLAT_DDR |= (1<<TLC5940_XLAT_BIT);


	while (1) {
		if (latchingFlag==0) {
			sendSerial();
			
			latchingFlag = 1;
		}
		for (int i=0;i<1000;i++)
			nop();
	}
	while (1) {}
}