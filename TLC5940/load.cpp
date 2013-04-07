#include <string.h>
#include "../shared/load.h"
#include "../shared/settings.h"
#include "strfmt.h"

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

#define PROGMEM_GRAPHIC
	
//~ static uint8_t nColumnData[SIDES_COUNT*SPOKES_COUNT][COLUMN_DATA_BYTES];
static uint8_t h = (VERTICAL_PIXELS<GRAPHIC_HEIGHT) ? VERTICAL_PIXELS : GRAPHIC_HEIGHT;
#define VOLREG

inline void padSPI(uint8_t nBitCount) {
	if (!nBitCount) return;
	SPCR &= ~(1<<SPE); // SPI off
	PORTB &= ~(1<<3);			// Data line (MOSI) low
	for (uint8_t i = 0; i<nBitCount; i++) {
		PORTB |= (1<<5);			// Clock high
		nop();
		PORTB &= ~(1<<5);		// Clock low
		nop();
	}
	SPCR |= (1<<SPE);	// Spi On
}

void loadingPrepareUpdate(uint8_t idxHorizontalPixel) {
	
	TLC5940_XLAT_PORT &= ~(1<<TLC5940_XLAT_BIT);		// XLAT -> low

	uint8_t idxUnit = 0;
	uint32_t nBitsSent = 0;
	
	if ( (idxHorizontalPixel<HORZ_PIXELS) && (idxHorizontalPixel<GRAPHIC_WIDTH)) {

		
		do { // This is the unit loop
			
			
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
			padSPI(0);
			SPDR = 0;					// First 8 bits of the empty 12-bit data
			while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
				;				
			SPDR = 0;					// Last 4 bits of the empty 12-bit data
										// plus the empty 4 high bits of the first intensity data
			while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
				;		
			nBitsSent += 16;
			
			
			// Different curColDatas are not implemented yet
			//~ uint8_t * curColData = nColumnData[idxUnit];
			const uint8_t * ptrGraphicFirst = &(graphic[idxHorizontalPixel][0]);
			const uint8_t * ptrGraphic = ptrGraphicFirst + h;
			
			VOLREG uint8_t idxChannel = 0;
			VOLREG uint8_t nPaletteIndex = pgm_read_byte(--ptrGraphic);
			VOLREG uint8_t dataByte = palette[nPaletteIndex*3+idxChannel++];
			VOLREG uint8_t toSPDR = dataByte;
			VOLREG uint8_t idxNextSendType = 1;
			VOLREG uint8_t nPixelsSent = 0;
			
			for (;;) { // Send bytes loop
				SPDR = toSPDR;			// Send the byte
				nBitsSent += 8;
				
				// Update the send type
				if (++idxNextSendType>2) { idxNextSendType = 0; }
				//~ ++idxNextSendType %= 4;
				
				// Prepare the next one for loading
				if (idxNextSendType==1) {
						// Case 1: Need to send the low nibble + padding
						toSPDR = dataByte<<4;
					
						/////////////////////////
						//~ toSPDR = 0x00; 
				} else {
					// Either way we need another new data in dataByte
					// 
					if (idxChannel==0) {
						// Load next palette index
						//~ if (--ptrGraphic < ptrGraphicFirst)
							//~ break;
						nPaletteIndex = pgm_read_byte(ptrGraphic);
						nPixelsSent++;
					}
					
					//~ dataByte = palette[nPaletteIndex*3+idxChannel];
					if (!idxChannel) {
						dataByte = 0xFF;
					} else {
						dataByte = 0;
					}
					
					if (++idxChannel>2) idxChannel = 0;
					//~ This fails: ++idxChannel %= 3;
					
					if (idxNextSendType) {
						// idxSendType == 2
						// Case 2: Need to send pure data
						toSPDR = dataByte;
						
						/////////////////////////
						//~ toSPDR = 0x00; 
						
					} else {
						// idxSendType == 0
						// Case 0: Need to send padding + high nibble
						toSPDR = dataByte >> 4;
						
						/////////////////////////
						//~ toSPDR = 0x00; 
					}
				}

				while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
					;				
				
				
#define PADDING
#ifdef PADDING
				//~ if (nPixelsSent && (nPixelsSent % 5) ) {
				if (nBitsSent % 192 == 0) {
					if (nBitsSent == 192 *3)
						break;
					
					//~ padSPI(0);
					
				//~ if ( (nBitsSent ==  372) || (nBitsSent ==  10) ) {
					SPDR = 0;					// First 8 bits of the empty 12-bit data
					while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
						;				
					SPDR = 0;					// Last 4 bits of the empty 12-bit data
												// plus the empty 4 high bits of the first intensity data
					while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
						;		
					idxNextSendType = 1;
					nBitsSent += 16;

				}
#endif				
			} // End of send bytes loop
			
			
		} while (++idxUnit < SIDES_COUNT*SPOKES_COUNT);	// 	End of the unit loop,
		
		// No need to pad, because the chips down the line will not get any data
	} else {
		// Send black
#if defined FIRST_WAY
		PORTB &= ~(1<<3);			// Data line low
		for (uint16_t i=0;i<144*8;i++) {
			PORTB |= (1<<5);			// Clock high
			nop();
			PORTB &= ~(1<<5);		// Clock low
		}
#elif defined DO_SOMETHING
		for (uint8_t i=0;i<144;i++) {
			SPDR = 0;			// Send the byte
			while(!(SPSR & (1<<SPIF))) 	// Wait for the transfer to finish
				;				
		}		
#else		
		// Do nothing
#endif		
		// Leave the data line low
		// doesn't work when SPI is enabled
		PORTB &= ~(1<<3);			// Data line low
	}

	
#define DEBUG_OUT	
#ifdef DEBUG_OUT
		//~ dputsi("VERTICAL_PIXELS: ", VERTICAL_PIXELS, 0);
		if (nBitsSent > 0) {
			dputs(" Bits sent: ", 0);
			dputi(nBitsSent, 0);
			dputs(" sent ", 1);
		}
#endif /* DEBUG_OUT */
}

void loadingUpdateDisplay(uint8_t idxHorizontalPixel) {
	TLC5940_XLAT_PORT |= 1<<TLC5940_XLAT_BIT;			// XLAT -> high
}

void loadingInitDisplay() {
	
	// Greyscale timer
	//~ Setting the COM2x1:0 bits to two will produce a non-inverted PWM and an inverted PWM output
	//~ can be generated by setting the COM2x1:0 to three
	DDRD |= 1<<3;		// OC2B is PB3, we are using PWM to toggle BLANK using that pin

	TCCR2A = 0
//		|(1<<COM2A1)|(0<<COM2A0)	// Clear OC2A on Compare Match, set OC2A at BOTTOM (non-inverting mode).
//									// Set OC2A on Compare Match, clear OC2A at BOTTOM, (inverting mode).
		|(1<<COM2B1)|(0<<COM2B0)	// Clear OC2B on Compare Match, set OC2B at BOTTOM (non-inverting mode).
									// Set OC2B on Compare Match, clear OC2B at BOTTOM, (inverting mode).
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
		//~ |(0<<SPR1)|(0<<SPR0)// Clock: Fosc/4
		//~ |(1<<SPR1)|(1<<SPR0)// Clock: Focs/128
		|(1<<SPR1)|(0<<SPR0)// Clock: Focs/64
		;
	SPSR = 0
		|(0<<SPI2X)			// Bit 0 – SPI2X: Double SPI Speed Bit
		//~ |(1<<SPI2X)			// Bit 0 – SPI2X: Double SPI Speed Bit
		;
		
}