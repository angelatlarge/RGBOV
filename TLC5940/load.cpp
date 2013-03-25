#include <string.h>
#include "../shared/load.h"
#include "../shared/settings.h"

/*
	To try:
		A. Load directly into the port (w/o the intermediate register)
		B. Avoid branching in loading
*/

#if GRAPHIC_HEIGHT>VERTICAL_PIXELS
#	define COLUMN_DATA_BYTES	(GRAPHIC_HEIGHT*3)
#else
#	define COLUMN_DATA_BYTES	(VERTICAL_PIXELS*3)
#endif

#if INTENSITY_LEVELS>1			
#error For TLC5940 INTENSITY_LEVELS must be set at zero
#endif


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

#define LEDS_PER_CHIP		5
#define CHIPS_PER_UNIT		6
#define LINES_PER_CHIP		(LEDS_PER_CHIP*3)

/*
	Unit = circuit (LED drivers) for one spoke, one side.
*/

#define SIN_UNIT0_CHIP0	1<<2
#define SIN_UNIT0_CHIP1	1<<1
#define SIN_UNIT0_CHIP2	1<<0
#define SIN_UNIT0_CHIP3	1<<3
#define SIN_UNIT0_CHIP4	1<<4
#define SIN_UNIT0_CHIP5	1<<5

#define SIN_UNIT1_CHIP0	1<<2
#define SIN_UNIT1_CHIP1	1<<1
#define SIN_UNIT1_CHIP2	1<<0
#define SIN_UNIT1_CHIP3	1<<3
#define SIN_UNIT1_CHIP4	1<<4
#define SIN_UNIT1_CHIP5	1<<5

#define SIN_UNIT2_CHIP0	1<<2
#define SIN_UNIT2_CHIP1	1<<1
#define SIN_UNIT2_CHIP2	1<<0
#define SIN_UNIT2_CHIP3	1<<3
#define SIN_UNIT2_CHIP4	1<<4
#define SIN_UNIT2_CHIP5	1<<5

#define SIN_UNIT3_CHIP0	1<<2
#define SIN_UNIT3_CHIP1	1<<1
#define SIN_UNIT3_CHIP2	1<<0
#define SIN_UNIT3_CHIP3	1<<3
#define SIN_UNIT3_CHIP4	1<<4
#define SIN_UNIT3_CHIP5	1<<5

#define DATAPORT_UNIT0	PORTC
#define DATAPORT_UNIT1	PORTC
#define DATAPORT_UNIT2	PORTC
#define DATAPORT_UNIT3	PORTC

uint8_t anChipOutputLine[SIDES_COUNT*SPOKES_COUNT][CHIPS_PER_UNIT] = {{1<<2, 1<<1, 1<<0, 1<<3, 1<<4, 1<<5}, {1<<2, 1<<1, 1<<0, 1<<3, 1<<4, 1<<5}, {1<<2, 1<<1, 1<<0, 1<<3, 1<<4, 1<<5}, {1<<2, 1<<1, 1<<0, 1<<3, 1<<4, 1<<5}};
volatile uint8_t* anChipOutPort[SIDES_COUNT*SPOKES_COUNT] = { &PORTC, &PORTC, &PORTC, &PORTC };
volatile uint8_t* anChipOutDDR[SIDES_COUNT*SPOKES_COUNT] = { &DDRC, &DDRC, &DDRC, &DDRC };
	
/*
#define TLC5940_SIN_BIT		0
#define TLC5940_SIN_PORT	PORTC
#define TLC5940_SIN_DDR		DDRC
#define TLC5940_SIN_PIN		PINC
*/
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

	
static uint8_t nColumnData[SIDES_COUNT*SPOKES_COUNT][COLUMN_DATA_BYTES];


void loadingPrepareUpdate(uint8_t idxHorizontalPixel) {
	
	TLC5940_XLAT_PORT &= ~(1<<TLC5940_XLAT_BIT);		// XLAT -> low

	uint8_t idxUnit = 0;
	
	if ( (idxHorizontalPixel<HORZ_PIXELS) && (idxHorizontalPixel<GRAPHIC_WIDTH)) {
		
		
		// Load graphic data and apply palette
		do { // This is the unit loop
		
			uint8_t * curColData = nColumnData[idxUnit];
			//~ uint8_t * curOutLines = anChipOutputLine[idxUnit];
			//~ volatile uint8_t * curOutPort = anChipOutPort[idxUnit];
			
				
				// Prepare a column to send
				/* 	if this array was reoriented we could save some time because
						a. there is an instruction to load PROGMEM data and increment the pointer
						b. we wouldn't have to compute array indeces every time.
				*/
				uint8_t h = (VERTICAL_PIXELS<GRAPHIC_HEIGHT) ? VERTICAL_PIXELS : GRAPHIC_HEIGHT;
				unsigned char * dataIndex = curColData;
				unsigned char * dataLimit = dataIndex + h*3;
				
				const uint8_t * ptrGraphic = &(graphic[idxHorizontalPixel][0]);
				do {	// This is the vertical pixel loop
				
	#				ifdef PROGMEM_GRAPHIC
						uint8_t nPaletteIndex = pgm_read_byte(ptrGraphic);
	#				else /* PROGMEM_GRAPHIC */
						uint8_t nPaletteIndex = *ptrGraphic;
	#				endif /* PROGMEM_GRAPHIC */
					ptrGraphic++;

					/*	Here we have an unrolled 0..2 loop: 
						it doesn't provide any speed savings at optlevel 3
						but it is not very ugly so we keep it			*/
	#				ifdef PRECOMPUTE_PALETTE
						*(dataIndex++) = palette[nPaletteIndex*3+0];
						*(dataIndex++) = palette[nPaletteIndex*3+1];
						*(dataIndex++) = palette[nPaletteIndex*3+2];
	#					else /* PRECOMPUTE_PALETTE */
						dataIndex++ = (1<<palette[nPaletteIndex*3+0]) - 1;
						dataIndex++ = (1<<palette[nPaletteIndex*3+1]) - 1;
						dataIndex++ = (1<<palette[nPaletteIndex*3+2]) - 1;
	#					endif /* PRECOMPUTE_PALETTE */
				} while (dataIndex<dataLimit);	// End of the vertical pixel loop
				
		} while (++idxUnit < SIDES_COUNT*SPOKES_COUNT);	/* 	End of the unit loop,
															because we unroll it for the loading */
			
				
		/* 	Now the column data array is a list of (exponentially corrected) intensities,
			currently 8bits/channel (though in the future it might be 12 bits/channel)
			with the TOP of the graphic being first in the nColumnData array
			and the bottom of the graphic being last								*/
		
		// Now load the data into the TLC5940
		
		for (uint8_t idxChan = LINES_PER_CHIP; idxChan-- > 0; ) {
			
			// Step 1: Send zeros for high 4 bits (pad the high bits)
				
			// Unrolled unit loop
			DATAPORT_UNIT0 = 0;
			DATAPORT_UNIT1 = 0;
			DATAPORT_UNIT2 = 0;
			DATAPORT_UNIT3 = 0;
			//~ (*curOutPort) = 0;
			// Unrolled bit loop
			// Bit 0
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high: Rising edge of the clock signal clocks in the data
			// Bit 1
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high: Rising edge of the clock signal clocks in the data
			// Bit 2
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high: Rising edge of the clock signal clocks in the data
			// Bit 3
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high: Rising edge of the clock signal clocks in the data
			
			// Step 2: Send real data for the low 8 bits
			/* 	This code assembles the bits from various parts of the graphic into a variable nSinData
				to be written to the output port.  So what we have the the following:
			
					* a variable nSinData
					* column data (sourceData), from which the bits need to be assembled into nSinData
					* a bit index or a bit mask according to which the bits are assembled
			
				We could think of three ways of loading data into nSinData
			
					A. if (sourceData & bitMask) { // set the bits in nSinData; }
					B. if (!(nSinData &bitMask) { // do step A }
					C. pure bit twiddle way.

				Way A is the first one I think of (KS).
				Way B takes advantage of the fact that once a bit goes high in our data, it stays high, 
				so we only need to look at sourceData is the corresponding bit in nSinData is low
				Way C avoids any branching (see http://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching)
				
				Conclusion: Way C is best if done right
				Execution time of way A & Way B is data dependent: Way A is best on black pixels (no bits set) and 
				Way B is best on white pixels (all bits are set).  Execution time of way C is independent of the data.
				Way A is better than Way B: Way A on Way A's worst case is faster than Way B on Way B's worst case
				Way C can be done a couple of ways.  To both set and clear bits, bit twiddling recommends
					nSinData ^= (-flag ^ nSinData) & source
				This turns out to be slower than way A. If you only need to SET the bits, but not clear them, then
					nSinData |= (-flag) & source
				is sufficient and executes about 4% faster than worst case Way A
				
				Therefore, we use the bit twiddling way
			*/
			uint8_t idxBit = 7;
			uint8_t nBitMask = 0x80; 	// Using a bitmask that's shifted down every iteration 
										// is faster than a bit index loop. 
			do {
				// Prepare a parallel set of bits to send

				TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
				
				// Unrolled unit loop
				
				/* ---UNIT 0 --- */
				uint8_t nSinData=0; /* 	Tested loading directly into port, and it was slower. 
										Storing the data in the intermediate data structure is more speed-efficient */
#define			COLUMNDATA_UNIT0	nColumnData[0]
				// Unrolled chips loop
				nSinData |= ((-(COLUMNDATA_UNIT0[0*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT0_CHIP0);
				nSinData |= ((-(COLUMNDATA_UNIT0[1*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT0_CHIP1);
				nSinData |= ((-(COLUMNDATA_UNIT0[2*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT0_CHIP2);
#if CHIPS_PER_UNIT>3
				nSinData |= ((-(COLUMNDATA_UNIT0[3*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT0_CHIP3);
				nSinData |= ((-(COLUMNDATA_UNIT0[4*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT0_CHIP4);
				nSinData |= ((-(COLUMNDATA_UNIT0[5*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT0_CHIP5);
#endif /* CHIPS_PER_UNIT>3 */
				DATAPORT_UNIT0		 = nSinData;					// Data
				
				/* ---UNIT 1 --- */
				nSinData=0; 
#define			COLUMNDATA_UNIT1	nColumnData[1]
				nSinData |= ((-(COLUMNDATA_UNIT1[0*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT1_CHIP0);
				nSinData |= ((-(COLUMNDATA_UNIT1[1*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT1_CHIP1);
				nSinData |= ((-(COLUMNDATA_UNIT1[2*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT1_CHIP2);
#if CHIPS_PER_UNIT>3
				nSinData |= ((-(COLUMNDATA_UNIT1[3*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT1_CHIP3);
				nSinData |= ((-(COLUMNDATA_UNIT1[4*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT1_CHIP4);
				nSinData |= ((-(COLUMNDATA_UNIT1[5*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT1_CHIP5);
				DATAPORT_UNIT1		 = nSinData;					// Data
				
				/* ---UNIT 2 --- */
				nSinData=0; 
#define			COLUMNDATA_UNIT2	nColumnData[2]
#endif /* CHIPS_PER_UNIT>3 */
				nSinData |= ((-(COLUMNDATA_UNIT2[0*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT2_CHIP0);
				nSinData |= ((-(COLUMNDATA_UNIT2[1*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT2_CHIP1);
				nSinData |= ((-(COLUMNDATA_UNIT2[2*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT2_CHIP2);
#if CHIPS_PER_UNIT>3
				nSinData |= ((-(COLUMNDATA_UNIT2[3*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT2_CHIP3);
				nSinData |= ((-(COLUMNDATA_UNIT2[4*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT2_CHIP4);
				nSinData |= ((-(COLUMNDATA_UNIT2[5*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT2_CHIP5);
#endif /* CHIPS_PER_UNIT>3 */
				DATAPORT_UNIT2		 = nSinData;					// Data
				
				/* ---UNIT 3 --- */
				nSinData=0; 
#define			COLUMNDATA_UNIT3	nColumnData[3]
				nSinData |= ((-(COLUMNDATA_UNIT3[0*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT3_CHIP0);
				nSinData |= ((-(COLUMNDATA_UNIT3[1*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT3_CHIP1);
				nSinData |= ((-(COLUMNDATA_UNIT3[2*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT3_CHIP2);
#if CHIPS_PER_UNIT>3
				nSinData |= ((-(COLUMNDATA_UNIT3[3*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT3_CHIP3);
				nSinData |= ((-(COLUMNDATA_UNIT3[4*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT3_CHIP4);
				nSinData |= ((-(COLUMNDATA_UNIT3[5*LINES_PER_CHIP+idxChan] & nBitMask >>idxBit)) & SIN_UNIT3_CHIP5);
#endif /* CHIPS_PER_UNIT>3 */
				DATAPORT_UNIT3		 = nSinData;					// Data
				
				idxBit--;
				//~ PORTC = nSinData;
				TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
				
				// Move to the next bitmask
				nBitMask >>= 1;
			} while (nBitMask);

		} // channel loop
	
		// End of sending real data
		
		
		
	} else {
		// We need only send padding now
		
		
		// Send black padding
		
		//~ (*curOutPort) = 0;
		DATAPORT_UNIT0 = 0;
		DATAPORT_UNIT1 = 0;
		DATAPORT_UNIT2 = 0;
		DATAPORT_UNIT3 = 0;

		for (uint8_t idxBit=0;idxBit<LINES_PER_CHIP*12;idxBit++) {  // Using "for" or a "while" loop is the same here
			TLC5940_SCLK_PORT 	&= ~(1<<TLC5940_SCLK_BIT);	// SCLK->low
			TLC5940_SCLK_PORT 	|= 1<<TLC5940_SCLK_BIT;		// SCLK->high 
															// Rising edge of the clock signal clocks in the data
		}
	}
}

void loadingUpdateDisplay(uint8_t idxHorizontalPixel) {
	TLC5940_XLAT_PORT |= 1<<TLC5940_XLAT_BIT;			// XLAT -> high
}

void loadingInitDisplay() {
	// Turn off the clock divisor
	CLKPR = 1<<CLKPCE;
	CLKPR = 0;
	
	
	// Set output pins
	for (int i=0; i<SIDES_COUNT*SPOKES_COUNT; i++) {
		*(anChipOutDDR[i]) = 0xFF;
	}
	TLC5940_XLAT_DDR |= 1<<TLC5940_XLAT_BIT;
	TLC5940_SCLK_DDR |= 1<<TLC5940_SCLK_BIT;
	TLC5940_DCPRG_DDR |= 1<<TLC5940_DCPRG_BIT;
	TLC5940_VPRG_DDR |= 1<<TLC5940_VPRG_BIT;

	// XLAT = low means data is ready to be activated, 
	// so we want to set XLAT high
	TLC5940_XLAT_PORT |= 1<<TLC5940_XLAT_BIT;			// XLAT -> high
	
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