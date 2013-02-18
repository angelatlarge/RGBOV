#include <string.h>
#include "../shared/load.h"
#include "../shared/settings.h"

#if GRAPHIC_HEIGHT>VERTICAL_PIXELS
#	define COLUMN_DATA_BYTES	(GRAPHIC_HEIGHT*3)
#else
#	define COLUMN_DATA_BYTES	(VERTICAL_PIXELS*3)
#endif

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

#define PRECOMPUTE_PALETTE		/* 	Optimization that does palette exponentiation at initialization.  
									Can saves about 51.2us of the loading time
									*/
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
#define CHIPS_COUNT		6
#define LINES_PER_CHIP	(LEDS_PER_CHIP*3)

uint8_t anChipOutputLine[CHIPS_COUNT] = {1<<2, 1<<1, 1<<0, 1<<3, 1<<4, 1<<5};

void doDisplayUpdate(uint8_t idxHorizontalPixel) {

	static uint8_t nColumnData[COLUMN_DATA_BYTES];
	
	if ( (idxHorizontalPixel<HORZ_PIXELS) && (idxHorizontalPixel<GRAPHIC_WIDTH)) {
		
		// Prepare a column to send
		/* 	if this array was reoriented we could save 2 clocks / loop
			because there is an instruction to load PROGMEM data and increment the pointer
		*/
		uint8_t h = (VERTICAL_PIXELS<GRAPHIC_HEIGHT) ? VERTICAL_PIXELS : GRAPHIC_HEIGHT;
		unsigned char * dataIndex = nColumnData;
		unsigned char * dataLimit = nColumnData + h*3;
		uint8_t idxRow=0;
		do {
		
		//~ for (int idxRow=0; idxRow<h; idxRow++) {
#			ifdef PROGMEM_GRAPHIC
				uint8_t nPaletteIndex = pgm_read_byte(&(graphic[idxRow++][idxHorizontalPixel]));
#			else /* PROGMEM_GRAPHIC */
				uint8_t nPaletteIndex = graphic[idxRow++][idxHorizontalPixel];
#			endif /* PROGMEM_GRAPHIC */
		
#define OPTIMIZE_UNROLLCOLORPALETTELOOP			/* No significant improvement from this optimization */
#ifdef OPTIMIZE_UNROLLCOLORPALETTELOOP
#				ifdef PRECOMPUTE_PALETTE
				*(dataIndex++) = palette[nPaletteIndex*3+0];
				*(dataIndex++) = palette[nPaletteIndex*3+1];
				*(dataIndex++) = palette[nPaletteIndex*3+2];
#				else /* PRECOMPUTE_PALETTE */
				dataIndex++ = (1<<palette[nPaletteIndex*3+0]) - 1;
				dataIndex++ = (1<<palette[nPaletteIndex*3+1]) - 1;
				dataIndex++ = (1<<palette[nPaletteIndex*3+2]) - 1;
#				endif /* PRECOMPUTE_PALETTE */
#else			
			// Copy the color stored in the palette into the column data
			for (int idxColor = 0; idxColor<3; idxColor++) {
				// The palette color is 3 bytes, so we need to copy 3 bytes
				// Secondly the palette color is stored as an intensity level, 
				//    a number in range 0-4, or 0-8, or 0-12
				// We need to convert that to 2^(intensity) - intensity is perceive exponentialy
#				ifdef PRECOMPUTE_PALETTE
				nColumnData[idxRow*3+idxColor] = palette[nPaletteIndex*3+idxColor];
#				else /* PRECOMPUTE_PALETTE */
				nColumnData[idxRow*3+idxColor] = (1<<palette[nPaletteIndex*3+idxColor]) - 1;
#				endif /* PRECOMPUTE_PALETTE */
				
			}
#endif			
		} while (dataIndex<dataLimit);
		
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
#undef OPTIMIZE_HIGHONCE			/* This turns out to be slower in practice. */
#ifdef OPTIMIZE_HIGHONCE
			uint8_t nSinData = 0;
			uint8_t nBitMask = 0x80; 	// Faster than a bit index loop
			do {
				if (!(nSinData & anChipOutputLine[0]) && (nColumnData[0*LINES_PER_CHIP+idxChan] & nBitMask)) nSinData |= anChipOutputLine[0]; // else: nSinData is already zero
				if (!(nSinData & anChipOutputLine[1]) && (nColumnData[0*LINES_PER_CHIP+idxChan] & nBitMask)) nSinData |= anChipOutputLine[1]; // else: nSinData is already zero
				if (!(nSinData & anChipOutputLine[2]) && (nColumnData[0*LINES_PER_CHIP+idxChan] & nBitMask)) nSinData |= anChipOutputLine[2]; // else: nSinData is already zero
#if CHIPS_COUNT>3
				if (!(nSinData & anChipOutputLine[3]) && (nColumnData[0*LINES_PER_CHIP+idxChan] & nBitMask)) nSinData |= anChipOutputLine[3]; // else: nSinData is already zero
				if (!(nSinData & anChipOutputLine[4]) && (nColumnData[0*LINES_PER_CHIP+idxChan] & nBitMask)) nSinData |= anChipOutputLine[4]; // else: nSinData is already zero
				if (!(nSinData & anChipOutputLine[5]) && (nColumnData[0*LINES_PER_CHIP+idxChan] & nBitMask)) nSinData |= anChipOutputLine[5]; // else: nSinData is already zero
#endif			
				TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
				TLC5940_SIN_PORT = nSinData;					// Data
				TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
			
				// Move to the next bitmask
				nBitMask >>= 1;
			} while (nBitMask);
#else						
			uint8_t nBitMask = 0x80; 	// Using a bitmask that's shifted down every iteration 
										// is faster than a bit index loop. 
			do {
				// Prepare a parallel set of bits to send
				uint8_t nSinData=0;
				// Unrolled chips loop
				if (nColumnData[0*LINES_PER_CHIP+idxChan] & (nBitMask)) nSinData |= anChipOutputLine[0]; // else: nSinData is already zero
				if (nColumnData[1*LINES_PER_CHIP+idxChan] & (nBitMask)) nSinData |= anChipOutputLine[1]; // else: nSinData is already zero
				if (nColumnData[2*LINES_PER_CHIP+idxChan] & (nBitMask)) nSinData |= anChipOutputLine[2]; // else: nSinData is already zero
#if CHIPS_COUNT>3
				if (nColumnData[3*LINES_PER_CHIP+idxChan] & (nBitMask)) nSinData |= anChipOutputLine[3]; // else: nSinData is already zero
				if (nColumnData[4*LINES_PER_CHIP+idxChan] & (nBitMask)) nSinData |= anChipOutputLine[4]; // else: nSinData is already zero
				if (nColumnData[5*LINES_PER_CHIP+idxChan] & (nBitMask)) nSinData |= anChipOutputLine[5]; // else: nSinData is already zero
#endif			
				TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
				TLC5940_SIN_PORT = nSinData;					// Data
				TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
				
				// Move to the next bitmask
				nBitMask >>= 1;
			} while (nBitMask);

#endif			
				
		} // channel loop
		
		TLC5940_XLAT_PORT |= 1<<TLC5940_XLAT_BIT;			// XLAT -> high
		
	} else {
		// Send black
		TLC5940_SIN_PORT 	= 0;
		
#define OPTB		
#ifdef OPTB		
		uint8_t idxBit = LINES_PER_CHIP*12;
		do {
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			--idxBit;
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
															// Rising edge of the clock signal clocks in the data
		} while (idxBit);
#else		
		for (uint8_t idxBit=0;idxBit<LINES_PER_CHIP*12;idxBit++) {
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
															// Rising edge of the clock signal clocks in the data
		}
#endif		
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

	// Precompute the palette
#	ifdef PRECOMPUTE_PALETTE
		for (int idxPal = 0; idxPal<PALETTE_SIZE; idxPal++) {
			for (int idxColor = 0; idxColor<3; idxColor++) {
				palette[idxPal*3+idxColor] = (1<<palette[idxPal*3+idxColor]) - 1;
			}
		}
#	else /* PRECOMPUTE_PALETTE */
		// Nothing to do 
#	endif /* PRECOMPUTE_PALETTE */
	
}