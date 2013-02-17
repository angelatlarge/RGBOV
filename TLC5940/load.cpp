#include <string.h>
#include "../shared/load.h"
#include "../shared/settings.h"

#define COLUMN_DATA_BYTES		VERTICAL_PIXELS*3

#if INTENSITY_LEVELS>1			
#error For TLC5940 INTENSITY_LEVELS must be set at zero
#endif

#define TLC5940_SIN_BIT		0
#define TLC5940_SIN_PORT	PORTC
#define TLC5940_SIN_DDR		DDRC
#define TLC5940_SIN_PIN		PINC

#define TLC5940_XLAT_BIT	7
#define TLC5940_XLAT_PORT	PORTD
#define TLC5940_XLAT_DDR	DDRD
#define TLC5940_XLAT_PINS	PIND

#define TLC5940_SCLK_BIT	4
#define TLC5940_SCLK_PORT	PORTD
#define TLC5940_SCLK_DDR	DDRD
#define TLC5940_SCLK_PINS	PIND

#define TLC5940_DCPRG_BIT	4
#define TLC5940_DCPRG_PORT	PORTB
#define TLC5940_DCPRG_DDR	DDRB
#define TLC5940_DCPRG_PINS	PINB

#define TLC5940_VPRG_BIT	5
#define TLC5940_VPRG_PORT	PORTB
#define TLC5940_VPRG_DDR	DDRB
#define TLC5940_VPRG_PINS	PINB

#define nop()  __asm__ __volatile__("nop")

#include "../shared/graphic.c"

/*
	We have a vertical line in PROGMEM
	Each byte in the vertical line is a palette index
	Each palette entry is 3 bytes, however, 
	we cannot send that directly.
	we must produce the intensity value from the pal idx by raising two to its power.
	If we are using 8-bit intensity, it is easy: each channel value is a byte (though in sending it must be padded anyway)
	If we are using 12-bit intensities, then each channel value is a byte+1 nibble, 
*/

#define LEDS_PER_CHIP	5
#define CHIPS_COUNT		3
#define LINES_PER_CHIP	(LEDS_PER_CHIP*3)

uint8_t anChipOutputLine[CHIPS_COUNT] = {1<<2, 1<<1, 1<<0};

void doDisplayUpdate(uint8_t idxHorizontalPixel) {

	//~ if (idxHorizontalPixel < 100) {
		//~ TLC5940_XLAT_PORT &= ~(1<<TLC5940_XLAT_BIT);		// XLAT -> low
		//~ for (int i =0; i<12*15; i++) {
			//~ TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			//~ nop();
			//~ TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
			//~ nop();
		//~ }
		//~ TLC5940_XLAT_PORT |= 1<<TLC5940_XLAT_BIT;			// XLAT -> high
		//~ return;
	//~ }
	
	static uint8_t nColumnData[COLUMN_DATA_BYTES];
	
	if ( (idxHorizontalPixel<HORZ_PIXELS) && (idxHorizontalPixel<GRAPHIC_WIDTH)) {
		// Prepare a column to send
		
		uint8_t h = (VERTICAL_PIXELS<GRAPHIC_HEIGHT) ? VERTICAL_PIXELS : GRAPHIC_HEIGHT;
		for (int idxRow=0; idxRow<h; idxRow++) {
#			ifdef PROGMEM_GRAPHIC
				uint8_t nPaletteIndex = pgm_read_byte(&(graphic[idxRow][idxHorizontalPixel]));
#			else /* PROGMEM_GRAPHIC */
				uint8_t nPaletteIndex = graphic[idxRow][idxHorizontalPixel];
#			endif /* PROGMEM_GRAPHIC */
			
			// Copy the color stored in the palette into the column data
			for (int idxColor = 0; idxColor<3; idxColor++) {
				// The palette color is 3 bytes, so we need to copy 3 bytes
				// Secondly the palette color is stored as an intensity level, 
				//    a number in range 0-4, or 0-8, or 0-12
				// We need to convert that to 2^(intensity) - intensity is perceive exponentialy
				nColumnData[idxRow*3+idxColor] = (1<<palette[nPaletteIndex*3+idxColor]) - 1;
			}
		}
		
		/* 	The column data array now is a list of (exponentially corrected) intensities,
			currently 8bits/channel (though in the future it might be 12 bits/channel)
			with the TOP of the graphic being first in the nColumnData array
			and the bottom of the graphic being last								*/
		
		// Now load the data into the TLC5940
		
		TLC5940_XLAT_PORT &= ~(1<<TLC5940_XLAT_BIT);		// XLAT -> low
		
		for (int idxChan = LINES_PER_CHIP-1; idxChan >= 0; idxChan--) {
			
			// Step 1: Send zeros for high 4 bits (pad the high bits)
			TLC5940_SIN_PORT 	= 0;
			for (uint8_t idxBit=0;idxBit<4;idxBit++) {
				TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
				TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
																// Rising edge of the clock signal clocks in the data
			}
			
			// Step 2: Send real data for the low 8 bits
			
			for (int idxBit = 7; idxBit>=0; idxBit--) {
				// Prepare a parallel set of bits to send
				uint8_t nSinData=0;
				for (int idxChip = 0 ; idxChip<CHIPS_COUNT; idxChip++) {
					if (nColumnData[idxChip*LINES_PER_CHIP+idxChan] & (1<<idxBit)) {
						nSinData |= anChipOutputLine[idxChip];
					} // else: nSinData is already zero
				}
			
				TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
				TLC5940_SIN_PORT = nSinData;					// Data
				TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
			}
				
		} // channel loop
		
		TLC5940_XLAT_PORT |= 1<<TLC5940_XLAT_BIT;			// XLAT -> high
		
		
	}
	
}

void doDisplayInit() {
	// Turn off the clock divisor
	CLKPR = 1<<CLKPCE;
	CLKPR = 0;
	
	
	// Set output pins
	TLC5940_SIN_DDR |= 0xFF;
	TLC5940_XLAT_DDR |= 1<<TLC5940_XLAT_BIT;
	TLC5940_SCLK_DDR |= 1<<TLC5940_SCLK_BIT;
	TLC5940_DCPRG_DDR |= 1<<TLC5940_DCPRG_BIT;
	TLC5940_VPRG_DDR |= 1<<TLC5940_VPRG_BIT;
	
	// Greyscale timer
	//~ Setting the COM2x1:0 bits to two will produce a non-inverted PWM and an inverted PWM output
	//~ can be generated by setting the COM2x1:0 to three
	DDRB |= 1<<3;		// OC2A is PB3, we are using PWM to toggle BLANK using that pin

	TCCR2A = 0
		|(1<<COM2A1)|(0<<COM2A0)	// Non-inverting mode
		|(1<<WGM21)|(1<<WGM20)		// Fast PWM
		;
	TCCR2B = 0
		|(0<<WGM22)							// Fast PWM
		|(0<<CS22)|(0<<CS21)|(1<<CS20)		// Prescaler = 1
		;
	OCR2A = 0x02;				/* 	If the OCR2A is set equal to BOTTOM, the output will
									be a narrow spike for each MAX+1 timer clock cycle. */
	//~ OCR2A = 0x80;

	// VPRG -> low: start with pwm mode
	TLC5940_VPRG_PORT &= ~(1<<TLC5940_VPRG_BIT);
	// DCPRG -> low
	TLC5940_DCPRG_PORT &= ~(1<<TLC5940_DCPRG_BIT);
		
}