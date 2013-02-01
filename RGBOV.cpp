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

# To do

* Retest flash memory

* Need to try smaller value resistors to make LEDs MOAR BRIGHT! MOAR SHINY!  ME LIKE SHINY!


# Some design notes

## Speeding up / slowing down

Currently how quickly we can react to speeding up slowing down
depends on the bike speed. At 15mpg, the slowest reaction is 1/3s
If we do not react quickly enough, the text/graphic will be stretched/shrunk
in the "horizontal" dimension.  Possible ways of solving:

* Moar brain! We could look at ac(/de)celeration and assume (for example), 
that ac(/de)celeration will be constant. Alternatively, we could assume that 
ac(/de)celeration will be half of what it was last time. 

An example of using the latter scheme: if after two switch hits the bike is computed to be going 10mph, 
and in the next switch hit the bike is computed to be moving at 12mph, 
you assume that for the next wheel rotation the bike will be going at 13mph. 
One advantage of this approach is that will not affect things 
when the speed is constant, so it seems like a pretty good solution.

Thinking the 1/2 ac(/de)celeration approach through a bit more, 
in the previous example displaying as if bike is moving at 13mph actually counts
acceleration as constant. Here's why: if during one rotation of the wheel, 
the bike accelerates from 12mph to 14mph, then in the middle of the cycle, 
the bike speed will be 13mph. So perhaps we should display the graphic 
for the speed which is current_speed + 1/4 * last_acceleration?

In order to know how this really works, we'd have to know ac(/de)celeration 
patters. It would be extra helpful to know this at various different speeds.
Perhaps RGBOV v1.0 can record this data, 
and transmit to the mothership for latter analysis?

Actually, if we think that there is acceleration/deceleration
the right thing to do is not necessarily use a constant speed, 
but rather change the update timer continuosely to match acceleration/deceleration.
Changing OCR1A while the timer is running is not entirely unproblematic
(even though we do this every time the hall effect switch hits).
Here's what the datasheet has to say on this topic:

However, changing the TOP to a value close to BOTTOM when the counter is running with none or a
low prescaler value must be done with care since the CTC mode does not have the double buffering
feature. If the new value written to OCR1A or ICR1 is lower than the current value of
TCNT1, the counter will miss the compare match. The counter will then have to count to its maximum
value (0xFFFF) and wrap around starting at 0x0000 before the compare match can occur.
In many cases this feature is not desirable. An alternative will then be to use the fast PWM mode
using OCR1A for defining TOP (WGM13:0 = 15) since the OCR1A then will be double buffered.

Returning to the idea of predicting future ac(/de)celeration, consider four scenarios
a. Constant speed
b. Constant ac(/de)celeration
c. Monotonically increasing ac(/de)celeration
d. Monotonically increasing ac(/de)celeration
e. Ac(/de)celeration reversal

Let's compare the "flat" algoritm (one that shows graphics based only on last speed) versus "predictive" algorithm.
The flag design will be the same as predictive in scenario a), worse in b) and c) and better in d) and e) 
[noting that e is just as subcase of d]. The question we need to answer is 
"in which regime outside of a) does a commuting biker spend more time?"
The acceleration/braking graphs do not make it clear, since there the riders were asked
to accelerate/decelerate as fast as possible, and those graphs make it clear that
POV display error can be enormous during rapid acceleration and deceleration.

* Moar sensor!  If our hall-effect switch was "stationary" (in the relevant sense), 
we could add more magnets in order to react to changing speeds quickly. However, our
sensor is rotating with the wheel itself, therefore, this will not work.  Possible workarounds:

	* Add more hall effect sensors.  Not as cheap as adding magnets, but doable. 
	With this scheme, a question arises as to when you adjust your text/graphic display:
	as soon as you know that the wheel spin isn't what you thought it was?  
	If this happens only with one sensor, the text/graphic never changes mid-display.
	With two sensors, immediate adjustment affects the graphic half-way through.

	* Put the sensor on the fork, and TRANSMIT sense info to the circuit on the wheel.
	Even though we will want the wheel to receive information from the stationary part of the bike, 
	this seems like a difficult solution: lots of communication between the wheel and the bike
	will eat computing cycles from the wheel circuit. We should consider this once we 
	work out the comms scheme between the bike and the wheel though.

	* Use a non-magnetic speed sensing mechanism. 

		* The most convenient thing would be to mount an optical sensor on the fork, 
		and sense when the spoke interrupts the light. However, this will also entail
		transmission of sense data from fork to wheel.

		* Can't think of a good way to do on-wheel optical sensing 
		(though I can think of several bad ways)

In the end, a combination of Moar Brains! and Moar Sensors might be the right solution. 
Even one more sensor will improve response two-fold, assuming that we can find the 
right way of handling of adjusting mid-graphic/text.

## LEDs

Kirill's RGB SMD LEDs are 5mmx5mm

## Memory

We need 220*30 = 6600 pixels.
At 1byte/pixel (4 intensity levels), this is is 6k6 bytes, more than anything but an ATmega1244 has for SRAM
At 1/2 bytes per pixel (1 bit/color = 8 colors), this is 3k3
At one bit per pixel (monochrome images) we need 825bytes.

Possible solutions:

	* Moar chip (ATmega1244, Xmega..)
	* ATmega8515 can have external memory
	* Storing data in Flash. Question is how fast?

Turns out that we can read images from flash memory fairly quickly, it seems.
ATmega328 has 32K of flash memory.  Last build had 6728 bytes of flash 
(which aready includes an 1.4K image), 
leaving 25k free. Assuming that the program size doubles, that's still enough
for two images at 1 byte per pixel (palletized). 
Loading new images may mean flashing a new program on, 
but that should not be a problem, and we might be able to fix that anyway.

## Bootloader

We might want to be able to load the program/PROGMEM data into the chip 
without an ISP programmer. For this we need a bootloader.  The way bootloaders
normally work is a) you reset the chip and mabe b) press some other button at the same time
When the chip is reset the bootloader runs, and if it desides the conditions are right (see b) above), 
it tries to download the new program. 
Or it could look for a program download within some timeout value.
Arduino has an auto-reset feature where the USB manipulates the DTS line
to reset the chip, and then the bootloader runs on reset.

All of this should be possible, however, it would be nice to be able to load
the graphic data onto the chip without loading the whole program. 
Assuming we can have the code and the data in different segments, 
and figure out the address of the data, it shouldn't be a problem, 
though it may mean writing some new bootloader code.

*/

/* 
	Timer usage
	Timer0 (8-bit):  Wheel speed measurement
	Timer1 (16-bit): Intensity/"horizontal" pixels timebase

	Two ways of handling any interrupt (hall effect switch, timer interrupt for sending pixels):
		A. Do everything necessary in the interrupt
		B. Set some "flag" and do everything in main
	We are doing A for the hall effect switch, and B for the pixel timer interrupt
		
*/


#include "glcdfont.c"

#define nop()  __asm__ __volatile__("nop")

#define	PIN_STCP		2
#define	PIN_SHCP		5
#define	PIN_DS			3

#undef USE_SR_CLASS
#undef USE_SR_SPI
#define USE_SR_MANUALBB

#undef PROGMEM_GRAPHIC
#define DRAW_INIT_HELLO
#define DRAW_WHEEL_SPEED

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

#define	HORZ_PIXELS				140
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

// uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS];
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
*/
// This is the colored blocks graphic

#ifdef PROGMEM_GRAPHIC
const uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS] PROGMEM = 
#else /* PROGMEM_GRAPHIC */
uint8_t graphic[VERTICAL_PIXELS][HORZ_PIXELS] = 
#endif /* PROGMEM_GRAPHIC */
{
//	 0     1     2     3     4     5     6     7     8     9    20     1     2     3     4     5     6     7     8     9    30     1     2     3     4     5     6     7     8     9  40
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


#if INTENSITY_LEVELS>1
uint8_t intensityMap[INTENSITY_LEVELS] = { 0, 1, 3, 7 };
#endif 	

#	ifndef PROGMEM_GRAPHIC
void eraseGraphic() {
	memset(&graphic[0][0], 00, VERTICAL_PIXELS*HORZ_PIXELS);
}
#endif /* PROGMEM_GRAPHIC */

#	ifndef PROGMEM_GRAPHIC
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
		x+=6;		/* 	Font is 5 pixels wide, need 1 pixel of space between letters....
						though this is odd, that should already be factored into the font itself.
						*/
	}
	return x;
}
#endif /* PROGMEM_GRAPHIC */

uint8_t checkUpdateWheelSpeed() {	/* Returns whether speed was updated */
	uint16_t nNewWheelTick = nHiResTimebaseCount;
	uint16_t nNewTickCount = nNewWheelTick - nLastWheelTick;
	if (nNewTickCount > 1000)	{		// This is a dumb-ass debouncing circuit
		nTicksPerRevolution = nNewTickCount;
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
#			ifndef PROGMEM_GRAPHIC
#				ifdef DRAW_WHEEL_SPEED			
					uint8_t nWF = fWheelFreq;
					char strWF[10];
					itoa(nWF, strWF, 10);
					char* strWFfact = strWF+1;
					strWFfact[0] = '.'; strWFfact++;
					itoa((fWheelFreq-nWF)*100, strWFfact, 10);
					int8_t x = 0;
					eraseGraphic();
					x = drawText(strWF, 0x00, 0x3F, x, 0);
					x = drawText(" hello, world!", 0x00, 0x3F, x, 0);
					//~ drawText(strWF, 0xFF, 00, 0, 0);
#				endif /* DRAW_WHEEL_SPEED */
#			endif /* PROGMEM_GRAPHIC */
			
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

#	ifndef PROGMEM_GRAPHIC
#		ifdef DRAW_INIT_HELLO	
			uint16_t x = 0;
			x=drawText("1234567890\0", 0, 0xFF, x, 0);
			/*
			x=drawText("Hel", 0, 3, x, 0);
			x=drawText("l\0", 0, 2, x, 0);
			x=drawText("l0\0", 0, 1, x, 0);
			x=drawText("world\0", 0, 48, x, 0);
			x=drawText("!\0", 0, 16, x, 0);
			*/
#		endif /* DRAW_INIT_HELLO */
#	endif /* PROGMEM_GRAPHIC */
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