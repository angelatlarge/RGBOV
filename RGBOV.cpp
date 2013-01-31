#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>
#include "sr595.h"
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include "serial.h"
#include "strfmt.h"
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


#include "glcdfont.c"

#define nop()  __asm__ __volatile__("nop")

#define	PIN_STCP		2
#define	PIN_SHCP		5
#define	PIN_DS			3

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


volatile	uint16_t	nHiResTimebaseCount;
volatile	uint16_t	nLastWheelTick;
volatile	uint16_t	nTicksPerRevolution;

volatile	uint32_t	nIntensityTimerHitCounter;

#define	HORZ_PIXELS				120
#define	INTENSITY_LEVELS		1
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
#if INTENSITY_LEVELS>1
volatile	uint8_t		idxIntensityTimeSlice;
#endif /* INTENSITY_LEVELS>1 */

uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS];
/*
= 
{
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00}
};
*/
/*
uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS] = 
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
*/
/*
uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS] = 
{
 {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 43, 23, 43, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63	,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 17, 17, 38, 63, 63, 17, 17, 17, 63, 63, 38, 17, 17, 63, 3, 3, 23, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 38, 17, 17, 63, 42, 17, 17, 17, 42, 63, 17, 17, 38, 63, 3, 3, 23, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 42, 17, 17, 59, 42, 17, 17, 17, 42, 42, 17, 17, 42, 63, 3, 3, 23, 3, 3, 23, 43, 63, 63, 58, 53, 48, 48, 48, 58, 63, 63, 62, 61, 60, 60, 60, 62, 63, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 42, 17, 17, 42, 38, 17, 17, 17, 38, 42, 17, 17, 63, 63, 3, 3, 3, 3, 3, 3, 23, 63, 58, 48, 48, 53, 53, 48, 48, 58, 62, 60, 60, 61, 61, 60, 60, 62, 63			,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 63, 17, 17, 38, 17, 17, 42, 17, 17, 38, 17, 38, 63, 63, 3, 3, 23, 63, 43, 3, 3, 63, 58, 48, 48, 58, 58, 48, 48, 53, 62, 60, 60, 62, 62, 60, 60, 61, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 63, 38, 17, 38, 17, 17, 42, 17, 17, 17, 17, 38, 63, 63, 3, 3, 23, 63, 43, 3, 3, 63, 53, 48, 48, 48, 48, 48, 48, 53, 61, 60, 60, 60, 60, 60, 60, 61, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 63, 42, 17, 17, 17, 38, 63, 17, 17, 17, 17, 42, 63, 63, 3, 3, 23, 63, 43, 3, 3, 63, 53, 48, 48, 58, 58, 58, 58, 58, 61, 60, 60, 62, 62, 62, 62, 62, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 63, 42, 17, 17, 17, 42, 63, 38, 17, 17, 17, 63, 63, 63, 3, 3, 23, 63, 43, 3, 3, 63, 58, 48, 48, 48, 53, 53, 48, 53, 62, 60, 60, 60, 61, 61, 60, 61, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {63, 63, 63, 17, 17, 17, 42, 63, 42, 17, 17, 38, 63, 63, 63, 3, 3, 23, 63, 43, 3, 3, 63, 63, 58, 48, 48, 48, 48, 48, 58, 63, 62, 60, 60, 60, 60, 60, 62, 63		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
};
uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS] = 
{
 {00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 43, 23, 43, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00	,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 17, 17, 38, 00, 00, 17, 17, 17, 00, 00, 38, 17, 17, 00, 3, 3, 23, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 38, 17, 17, 00, 42, 17, 17, 17, 42, 00, 17, 17, 38, 00, 3, 3, 23, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 42, 17, 17, 59, 42, 17, 17, 17, 42, 42, 17, 17, 42, 00, 3, 3, 23, 3, 3, 23, 43, 00, 00, 58, 53, 48, 48, 48, 58, 00, 00, 62, 61, 60, 60, 60, 62, 00, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 42, 17, 17, 42, 38, 17, 17, 17, 38, 42, 17, 17, 00, 00, 3, 3, 3, 3, 3, 3, 23, 00, 58, 48, 48, 53, 53, 48, 48, 58, 62, 60, 60, 61, 61, 60, 60, 62, 00			,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 00, 17, 17, 38, 17, 17, 42, 17, 17, 38, 17, 38, 00, 00, 3, 3, 23, 00, 43, 3, 3, 00, 58, 48, 48, 58, 58, 48, 48, 53, 62, 60, 60, 62, 62, 60, 60, 61, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 00, 38, 17, 38, 17, 17, 42, 17, 17, 17, 17, 38, 00, 00, 3, 3, 23, 00, 43, 3, 3, 00, 53, 48, 48, 48, 48, 48, 48, 53, 61, 60, 60, 60, 60, 60, 60, 61, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 00, 42, 17, 17, 17, 38, 00, 17, 17, 17, 17, 42, 00, 00, 3, 3, 23, 00, 43, 3, 3, 00, 53, 48, 48, 58, 58, 58, 58, 58, 61, 60, 60, 62, 62, 62, 62, 62, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 00, 42, 17, 17, 17, 42, 00, 38, 17, 17, 17, 00, 00, 00, 3, 3, 23, 00, 43, 3, 3, 00, 58, 48, 48, 48, 53, 53, 48, 53, 62, 60, 60, 60, 61, 61, 60, 61, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 },
 {00, 00, 00, 17, 17, 17, 42, 00, 42, 17, 17, 38, 00, 00, 00, 3, 3, 23, 00, 43, 3, 3, 00, 00, 58, 48, 48, 48, 48, 48, 58, 00, 62, 60, 60, 60, 60, 60, 62, 00		,00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 }
};
uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS] = 
{
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00},
 {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00}
};
*/

#if INTENSITY_LEVELS>1
uint8_t intensityMap[INTENSITY_LEVELS] = { 0, 1, 3, 7 };
#endif 	

void eraseGraphic() {
	memset(&graphic[0][0], 00, VERTICAL_PIXELS*HORZ_PIXELS);
}

//~ void drawText(uint8_t pMemory[VERTICAL_PIXELS][HORZ_PIXELS], uint8_t w, uint8_t h, char * str, uint8_t bg, uint8_t fg, int16_t x, int16_t y) {
uint16_t drawText(const char * str, uint8_t bg, uint8_t fg, int16_t x, int16_t y) {
	uint8_t idxChar = 0;
	while (str[idxChar]) {
		for (int8_t i=0; i<6; i++ ) {	// X LOOP i is the x dimension in the font
			uint8_t line;
			if (i == 5) 
				line = 0x0;
			else {
				line = pgm_read_byte(font+(str[idxChar]*5)+i);
			}
			
			for (int8_t j = 0; j<8; j++) {	// Y loop
				if ((x+i<HORZ_PIXELS) && (y+j<VERTICAL_PIXELS)) {
				
					graphic[y+j][x+i] = (line & 0x1) ? fg : bg;
				}
				//~ memset((&pMemory[0][0])+x+i+(y+j)*w, (line & 0x1) ? fg : bg, 1);
				//~ pMemory[y+j][x+i] = (line & 0x1) ? fg : bg;
					//~ uint8_t * pTarget = &(pMemory[y+j][x+i]);
					//~ uint8_t * pTarget = &(pMemory[x+i+(y+j)*w]);
				/*
					if (line & 0x1) {
						//~ pMemory[x+i+(y+j)*w] = fg;
						pMemory[y+j][x+i] = fg;
						//~ *pTarget = fg;
						//~ pMemory[y+j][x+i] = fg;
					} else if (bg != fg) {
						pMemory[y+j][x+i] = fg;
						*pTarget = bg;
						//~ pMemory[x+i+(y+j)*w] = bg;
					}
				*/
				//~ }
				
				line >>= 1;
			}
		}
		idxChar++;
		x+=5;
	}
	return x;
}

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
			float fWheelFreq = WHEEL_SPEED_TIMER_FREQ_FLOAT/(float)nTicksPerRevolution;		// Wheel frequency
			uint8_t nWF = fWheelFreq;
			char strWF[10];
			itoa(nWF, strWF, 10);
			int8_t x = 0;
			eraseGraphic();
			for (int i=0; i<5; i++) {
				x = drawText(strWF, 0x00, 0x3F, x, 0);
				x = drawText(" \0", 0x00, 0x3F, x, 0);
			}
			//~ drawText(strWF, 0xFF, 00, 0, 0);
			
			// We want the intensity timer tick to occur 2^(INTENSITY_LEVELS) times per horizontal pixel
			OCR1A = round(INTENS_TIMER_BASEFREQ_FLOAT/fWheelFreq/(float)(INTENSITY_COUNTER_MAX)/(float)HORZ_PIXELS);		
			
			dputsi("INTENSITY_COUNTER_MAX: ", INTENSITY_COUNTER_MAX);

			// 3. Reset the pixel position
#			if INTENSITY_LEVELS>1			
				idxHorizontalPixel = idxIntensityTimeSlice = 0;
#			else			
				idxHorizontalPixel = 0;
#			endif /* INTENSITY_LEVELS>1 */
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
	if (idxHorizontalPixel<HORZ_PIXELS) {
		// Prepare a column to send
		uint8_t idxOutBit = 0;
		uint8_t idxOutByte = 0;
		for (int idxRow=0; idxRow<VERTICAL_PIXELS; idxRow++) {
			for (int idxColor = 4; idxColor>=0; idxColor-=2) {	/* start with red (which is binary 00110000) */
				uint8_t nColorIntensity = ((0x03<<idxColor) & graphic[idxRow][idxHorizontalPixel]) >> idxColor;
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
				PORTB = (1<<PIN_DS)|(1<<PIN_SHCP);		// Raise DS 
			} else {
				PORTB = 1<<PIN_SHCP;
			}
		}
	}
	PORTB = 1<<PIN_STCP;					// Raise STCP
#else
#error Cant use shift registers	
#endif	
}

ISR(TIMER1_COMPA_vect ) {
	// Timebase for intensity timer hit
	nIntensityTimerHitCounter++;
}



int main() {
	serial_init();
	puts("Hello, world");

	//~ drawText(&graphic[0][0], 80, 10, "hello, world\0", 0, 3, 0, 0);
	//~ drawText(graphic, 80, 10, "hello, world\0", 0, 3, 0, 0);

	uint16_t x = 0;
	x=drawText("Hel", 0, 3, x, 0);
	x=drawText("l\0", 0, 2, x, 0);
	x=drawText("l0\0", 0, 1, x, 0);
	x=drawText("world\0", 0, 48, x, 0);
	x=drawText("!\0", 0, 16, x, 0);

	//~ void drawText(uint8_t* pMemory/*[VERTICAL_PIXELS][HORZ_PIXELS]*/, uint8_t w, uint8_t h, char * str, uint8_t bg, uint8_t fg, int16_t x, int16_t y) {
	
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