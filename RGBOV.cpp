#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>
#include "sr595.h"
#include "string.h"
#include <util/delay.h>
/* 
	Timer0 (8-bit):  Wheel speed measurement
	Timer1 (16-bit): Intensity timebase
		OCR1A: Wheel speed
		OCR1B: Intensity

	Two ways of handling hall effect switch:
		A. Do everything necessary in the interrupt
		B. Set some "flag" and do everything in main
		
	Memory issues
		We need 220*30 = 6600 pixels
		Even at 1byte/pixel that is 6k6 bytes, more than anything but an ATmega1244 has
		(At one bit per pixel (monochrome images) we need 825bytes.
		Possible solutions:
			* Moar chip (ATmega1244, Xmega..)
			* ATmega8515 can have external memory
			* Storing data in Flash. Question is how fast?
*/


const uint8_t STCP[4] = {4, 4, 4, 4};
sr595 sr(
		4, 					// nCascadeCount
		0, 					// fParallel
		&PORTB,		        // ptrPort
		&DDRB, 				// ptrDir
		7,	 				// nOE
		0, 					// Invert OE
		3,			 		// nDS
		5, 					// nSHCP
		STCP				// anSTCP
		);


volatile	uint16_t	nHiResTimebaseCount;
volatile	uint16_t	nLastWheelTick;
volatile	uint16_t	nTicksPerRevolution;

volatile	uint32_t	nIntensityTimerHitCounter;

#define	HORZ_RESOLUTION			100
#define	INTENSITY_LEVELS		4
#define	INTENSITY_COUNTER_MAX	1<<(INTENSITY_LEVELS-1)
#define VERTICAL_PIXELS			10
#define COLUMN_DATA_BYTES	(VERTICAL_PIXELS*3/8+( (VERTICAL_PIXELS*3%8) ? 1 : 0))

#define WHEEL_SPEED_TIMER_OCR			4
#define WHEEL_SPEED_TIMER_PRESCALER		256
#define WHEEL_SPEED_TIMER_PRESC_BITS	(1<<CS02)|(0<<CS01)|(0<<CS00)
#define WHEEL_SPEED_TIMER_FREQ_INT		(F_CPU/WHEEL_SPEED_TIMER_PRESCALER/WHEEL_SPEED_TIMER_OCR)
#define WHEEL_SPEED_TIMER_FREQ_FLOAT	((float)F_CPU/(float)WHEEL_SPEED_TIMER_PRESCALER/(float)WHEEL_SPEED_TIMER_OCR)

#define INTENS_TIMER_PRESCALER			1
#define INTENS_TIMER_PRESC_BITS			(0<<CS12)|(0<<CS11)|(1<<CS10)
#define INTENS_TIMER_BASEFREQ_FLOAT		((float)F_CPU/(float)INTENS_TIMER_PRESCALER)

volatile	uint8_t		idxHorizontalPixel;
volatile	uint8_t		idxIntensityTimeSlice;

uint8_t graphic[VERTICAL_PIXELS][HORZ_RESOLUTION] = 
{
 {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 36, 36, 36, 36, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 00, 36, 36, 36, 36, 36, 00, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 36, 32, 32, 36, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 32, 36, 36, 32, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 36, 36, 36, 32, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 36, 36, 32, 36, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 36, 36, 36, 36, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 00, 36, 36, 32, 36, 36, 00, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {32, 36, 36, 36, 36, 36, 36, 36, 36, 36, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00}
};

uint8_t intensityMap[INTENSITY_LEVELS] = { 0, 1, 3, 7 };
	

uint8_t checkUpdateWheelSpeed() {	/* Returns whether speed was updated */
	uint16_t nNewWheelTick = nHiResTimebaseCount;
	if ((nNewWheelTick - nLastWheelTick) > 1000)	{		// This is a dumb-ass debouncing circuit
		nTicksPerRevolution = nNewWheelTick - nLastWheelTick;
		nLastWheelTick = nNewWheelTick;
		return 1;
	} 
	return 0;
}

ISR(TIMER0_COMPA_vect ) {
	// Timebase for wheel speed
	nHiResTimebaseCount++;
}

ISR(INT0_vect) {
	
	// Occurs when the hall switch is triggered
	
	// 1. Calculate the new wheel speed
	if (checkUpdateWheelSpeed()) {
	
		// 2. Set up the timer interrupt 
		if (nTicksPerRevolution > 0) {
			float fWheelFreq = (float)nTicksPerRevolution / WHEEL_SPEED_TIMER_FREQ_FLOAT;		// Wheel frequency
			// We want the intensity timer tick to occur 2^(INTENSITY_LEVELS) times per horizontal pixel
			OCR1A = round(INTENS_TIMER_BASEFREQ_FLOAT/fWheelFreq/(float)(1<<INTENSITY_LEVELS)/(float)HORZ_RESOLUTION);		
			
			// 3. Reset the pixel position
			idxHorizontalPixel = idxIntensityTimeSlice = 0;
		} /*
			else: 
				potentially dividing by zero when computing the necessary intensity timer frequency
		}
		*/
	}		
}

void doDisplayUpdate() {
	
	static uint8_t nColumnData[COLUMN_DATA_BYTES];
	
	memset(nColumnData, 0, sizeof(nColumnData));
	if (idxHorizontalPixel<HORZ_RESOLUTION) {
		// Prepare a column to send
		uint8_t idxOutBit = 0;
		uint8_t idxOutByte = 0;
		for (int idxRow=0; idxRow<VERTICAL_PIXELS; idxRow++) {
			for (int idxColor = 4; idxColor>=0; idxColor-=2) {	/* start with red (00110000) */
				uint8_t nColorIntensity = (0x03<<idxColor) & graphic[idxRow][idxHorizontalPixel] >> idxColor;
				uint8_t nColorIntTS = intensityMap[nColorIntensity];
				nColumnData[idxOutByte] = 0;
				if (nColorIntTS>=idxIntensityTimeSlice) {
					// This color on this pixel is still ON
					nColumnData[idxOutByte] |= 1<<idxOutBit;
				}	
				if (++idxOutBit>7) {
					idxOutByte++;
					idxOutBit=0;
				}
			}
		}
		
		// Move to the next intensity
		if (++idxIntensityTimeSlice >= INTENSITY_COUNTER_MAX) {
			idxIntensityTimeSlice = 0;
			idxHorizontalPixel++;
		}
		
	} /*
		else {
			The column data is already zeros (see memset above), 
			so just send that
		}
	*/
	
	// Send the column
	sr.forceWriteData(0, COLUMN_DATA_BYTES, nColumnData);	
}

ISR(TIMER1_COMPA_vect ) {
	// Timebase for intensity timer hit
	nIntensityTimerHitCounter++;
}




int main() {
	
	// Wheel speed measurement timer = timer 0
	TCCR0A = 0 
		|(1<<WGM01)|(0<<WGM00)		// CTC mode
		;
	TCCR0B = 0
		| (1<<WGM02) 				
		| WHEEL_SPEED_TIMER_PRESC_BITS	
		;
	OCR0A = WHEEL_SPEED_TIMER_OCR;	// Timer 0 frequency = (8Mhz/256*4  = 7812.5Hz
										/*	At 30mph and 26" wheel size, 
											the interrupt will be triggered 1302 times/revolution
										*/
	TIMSK0 |= 1<<OCIE0A;					// Match A interrupt enable
	

	// Wheel magnetic switch interrupt enable
	EICRA = (1<<ISC01) | (1<<ISC00);		// Falling edge of int0 triggers interrupt
	EIMSK |= (1<<INT0);
	
	// Intensity timer
	TCCR1A = 0
		|(0<<WGM11)|(0<<WGM10)				// CTC mode
		;
	TCCR1B = 0
		|(INTENS_TIMER_PRESC_BITS)			// Prescaler
		|(0<<WGM13)|(1<<WGM12)				// CTC Mode
		;
	OCR1A = 0xFFFF;							// Not doing anything until we get a wheel speed
	TIMSK1 = (1<<OCIE1A);					// Enable interrupt on timer1

	sei();									// Enable interrupts
	
	DDRB = 0xFF;
	DDRD = 0x00;		// Set all of port D as input
	//~ PORTD = 0xFF;		// Pull up resistors on all PORTD pins


	//~ uint8_t anData[4] = {0x00, 0x00, 0x00, 0x00};
	//~ sr.forceWriteData(0, 4, anData);	
	nTicksPerRevolution = 0;

	for (;;) {
		if (nIntensityTimerHitCounter != 0) {
			nIntensityTimerHitCounter = 0;
			doDisplayUpdate();
		}
	}
		
		//~ for (int idxByte = 0; idxByte<4; idxByte++) {
			//~ for (int j = 0; j<8; j++) {
				//~ uint8_t nData[4] = {0x00, 0x00, 0x00, 0x00};
				//~ nData[idxByte] = 01<<j;
				//~ sr.forceWriteData(0, 4, nData);	
				//~ sr.writeByte(idxByte, nData[idxByte]);
				//~ _delay_ms(250);
				//~ nData[idxByte] = 0;
			//~ }
		//~ }
		//~ sr.writeByte(0, 01);
		/*
		for (
		
		uint8_t nDataA[4] = {0xFF, 0xFF, 0xFF, 0xFF};
		//~ for (int i = 0; i<4; i++) {
			//~ sr.writeByte(i, nDataA[i]);
		//~ }
			sr.writeData(0, 4, nDataA);	
		 _delay_ms(1000);
		
		uint8_t nDataB[4] = {0x00, 0x00, 0x00, 0x00};
		//~ for (int i = 0; i<4; i++) {
			//~ sr.writeByte(i, nDataB[i]);
		//~ }
		sr.writeData(0, 4, nDataB);	
		 _delay_ms(1000);
	*/
	//~ }
}