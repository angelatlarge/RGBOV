#define	PIN_STCP		2
#define	PIN_SHCP		5
#define	PIN_DS			3

#include "../shared/load.h"
#include "../shared/settings.h"
#include <string.h>

#undef USE_SR_CLASS
#undef USE_SR_SPI
#define USE_SR_MANUALBB


#ifdef USE_SR_CLASS
const uint8_t STCP[4] = {PIN_STCP, PIN_STCP, PIN_STCP, PIN_STCP};
sr595 sr(
		4, 					// nCascadeCount
		0, 					// fParallel
		&PORTB,		        // ptrPort
		&DDRB, 				// ptrDir
		7,	 				// nOE
		0, 					// Invert OE
		PIN_DS,			 		// nDS
		PIN_SHCP, 					// nSHCP
		STCP				// anSTCP
		);
#endif

#include "../shared/graphic.c"

void doDisplayUpdate(
	uint8_t idxHorizontalPixel
#	if INTENSITY_LEVELS>1			
	, uint8_t idxIntensityTimeSlice
#	endif /* INTENSITY_LEVELS>1 */
	) {
	
	static uint8_t nColumnData[COLUMN_DATA_BYTES];
	
	memset(nColumnData, 0, sizeof(nColumnData));
	if (idxHorizontalPixel<HORZ_PIXELS) {
		// Prepare a column to send
		uint8_t idxOutBit = 0;
		uint8_t idxOutByte = 0;
		for (int idxRow=0; idxRow<VERTICAL_PIXELS; idxRow++) {
			for (int idxColor = 4; idxColor>=0; idxColor-=2) {	/* start with red (which is binary 00110000) */
#				ifdef PROGMEM_GRAPHIC
					uint8_t nColorIntensity = ((0x03<<idxColor) & pgm_read_byte(&(graphic[idxRow][idxHorizontalPixel]))) >> idxColor;
#				else /* PROGMEM_GRAPHIC */
					uint8_t nColorIntensity = ((0x03<<idxColor) & graphic[idxRow][idxHorizontalPixel]) >> idxColor;
#				endif /* PROGMEM_GRAPHIC */
#				if INTENSITY_LEVELS == 1
					// No PWM
					if (nColorIntensity) {
						nColumnData[idxOutByte] |= 1<<idxOutBit;
					} /* else {
						Nothing to do: this bit is already zero
					} */
#				else
					// doing PWM
					uint8_t nColorIntTS = intensityMap[nColorIntensity];
					if (nColorIntTS>idxIntensityTimeSlice) {
						// This color on this pixel should be ON
						nColumnData[idxOutByte] |= 1<<idxOutBit;
					} /*
						else:
							this bit is already zero, see memset above
					*/
#				endif /* Do PWM? */					

				// Move on to the next bit (and maybe byte)
				if (++idxOutBit>7) {
					idxOutByte++;
					idxOutBit=0;
				}
			}
		}
		
#		if INTENSITY_LEVELS == 1
			idxHorizontalPixel++;
#		else			
			// Move to the next intensity
			if (++idxIntensityTimeSlice >= INTENSITY_COUNTER_MAX) {
				idxIntensityTimeSlice = 0;
				idxHorizontalPixel++;
			}
#		endif /* PWM? */		
	} /*
		else {
			The column data is already zeros (see memset above), 
			so just send that
		}
	*/
	
	// Send the column
#if defined USE_SR_CLASS
	sr.forceWriteData(0, COLUMN_DATA_BYTES, nColumnData);	
#elif defined USE_SR_SPI
	//~ PORTB &= ~(1<<PIN_STCP);					// Lower STCP
	PORTB=0;
	for (int i=0; i<COLUMN_DATA_BYTES; i++) {
		SPDR = nColumnData[3-i];
		while (!(SPSR & (1<<SPIF)));		// Wait for the data to be sent
	}	
	PORTB = (1<<PIN_STCP);					// Raise STCP
	//~ PORTB &= ~(1<<PIN_SHCP);					// Lower SHCP
	//~ nop();
	//~ PORTB &= ~(1<<PIN_STCP);				// Lower STCP
	//~ PORTB |= (1<<PIN_STCP);					// Raise STCP
	//~ PORTB = 0;
	
#elif defined USE_SR_MANUALBB
	/* Fastest bit-banging algorithm I could come up with */
	for (int i=0; i<COLUMN_DATA_BYTES; i++) {
		for (int nBit=7; nBit>=0; nBit--) {
			PORTB = 0; 						// Lower SHCP... PORTB &= ~(1<<PIN_SHCP) works too, but this should be a hair faster
			if (nColumnData[3-i] & (0x01 << nBit)) {
				PORTB = (1<<PIN_DS)|(1<<PIN_SHCP);		// Raise DS and CLOCK
			} else {
				PORTB = 1<<PIN_SHCP;					// Raise just the CLOCK
			}
		}
	}
	PORTB = 1<<PIN_STCP;					// Raise STCP
#else
#error Cant use shift registers	
#endif	
}

void doDisplayInit() {
	// Set up the SPI
#ifdef USE_SR_SPI
	SPCR = 0
		|(0<<SPIE)		// Interrupt 
		|(1<<SPE)		// SPI enable
		|(1<<DORD)		// MSB first
		|(1<<MSTR)		// Master
		|(0<<CPOL)		// Clock polarity: When this bit is written to one, SCK is high when idle.
		|(0<<CPHA)		// Clock leading/trailing edge
		|(1<<SPR1)|(1<<SPR0)	// F_mhz/4
		;
	SPSR = 0
		|(1<<SPI2X)		// Speed doubling
		;
#	endif		
	
	
	DDRB = 0xFF;
	DDRD = 0x00;		// Set all of port D as input
	//~ PORTD = 0xFF;		// Pull up resistors on all PORTD pins
	
}