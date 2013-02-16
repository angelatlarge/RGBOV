#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>
#include "sr595.h"
#include <stdlib.h>
#include <util/delay.h>
#include "serial.h"
#include "strfmt.h"
#include "load.h"
#include "settings.h"

/*

# To do

* Need to try smaller value resistors to make LEDs MOAR BRIGHT! MOAR SHINY!  ME LIKE SHINY!


# Some design notes

## How much current can batteries source?

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

## Memory and "scanline decomposition"

Right now we calculate the vertical scanline that needs to be sent every time.  
This takes time, but saves on image space. Currently a 200x10 image at 1 byte per pixel takes up 2K bytes of memory.
If we precalculate all the possible scanlines, the size of the image in memory would be 3K, 
however computing resources would be saved.

We will have 30*3=90 bits of data to send for each scanline.  
How much time will this take in terms of clock cycles?  
Well, even if we hand-code this in assemble, when bitbanging, 
for each bit we have to have:

	* An if statement (is the bit high or low?)
	* Outputting of the data on pins
	* Loop control.

In pseudocode it might look like this:

	is bit high?
		yes: raise DS AND STCP
	else
		no: raise just the STCP
	loop until condition

So three clocks/bit is the absolute bare minimum. This means 270 clock cycles for each scanline. 
Assume that our processor runs at 20Mhz, that we have 200 horizontal pixels, 
and maximum speed is 30mph = 6Hz wheel frequency.  This means that if we do only one update/pixel (no PWM)
we have 20Mhz/(6Hz*200pixels)=16000 clock cycles between each pixel. 
The minimum we need to do for 4 intensity levels is 16 updates /pixel, 
which gives us 1000 clocks between updates, but this starts to approach the limit of what we can do:
If our update routine is not 100% efficient it might take more than three clocks/bit.
Doubling the update rate to 32/pixel gives us 500 clocks between updates, so that could be doable, 
but it would seem that the image would still appear pixelated. 
Further doubling is impossible, as 64 updates/pixel only leaves 250 clock cycles for each update, which is not enough.

### Parallel loading

While each bit in a shift register MUST BE loaded serially, we can load the shift registers in parallel. 

#### Parallel loading: Setup A

7 shift registers have their DS (data) line connected to separate pins of the same port, 
and all of them have their clock (SHCP) line on the same (eighth) pin. 
Assuming that data line and clock line can be set silmutaneosely reliably 
(that is what we do now), the pseudocode, for scanline-decomposed image will look like this:

	Loop:
		Clear port A
		(Clear port B)
		Load data + STCP on port A A
		(Load data + STCP lines B)
		...
		Loop control

Theoretically, this will allow us to load 7 bits of data every three clock 
and 14 bits of data every five clock cycles.  
We need 90 bits of data for each scanline, which will require 12 shift registers (SRs),
so lets assume two parallel banks, which will give us up to 14 SRs. 
Full load will require 8 loop iterations, one for each bit.
At five clocks/loop iteration, the maximum performance here is only 40 clocks, 
although we will pay for it in memory space 
by not being able to use one bit out of every 8.
if, as before at 20Mhz, 30mph(=6Hz), 200 horz pixels we have 16K cycles / pixel, 
with this method we might be able to do 265 cycles/pixel:
At 265 cycles/pixel, each cycle will have 62 clocks: 
if we only need 40 clock to push out the data, then this looks more doable, 
though it is a bit tight.
256 cycles are needed to produce 8 intensity levels, 
but presumably it will make 4-level graphics not look pixellated.

#### Parallel loading: Setup A

It is also possible that data really must be loaded PRIOR to clock line (SHCP) going high, 
then we might put all clock lines on one port, and have a port for each bank of data lines.
Now each bank can have 8 up to SRs, meaning 16 bits of info out for each loop
The pseudocode  would look like this:

	Loop:
		Clear port A
		Clear port B
		Load data + STCP on port A A
		Load data + STCP lines B
		Raise the clock line
		Loop control

This means a minimum of 6 clocks for each bit, meaning 48 clocks minimum to load (up to) 
128 bits or 42 RGB leds.

### Instruction execution times

So after looking at the datasheet a bit longer, the assumption of 1 clock/instruction seem unwarranted
(I believed you Atmel!).  Let's try to annotate the last pseudocode with clock times

	Loop:
		OUT				# Lower SHCP, 1 clock
		LPM				# Load program memory to register for bank A + advance, 3 clocks
		LPM				# Load program memory to register for bank B + advance, 3 clocks
		OUT				# Output bank A, 1 clock
		OUT				# Output bank B, 1 clock
		OUT				# Raise SHCP, 1 clock
		???				# Decrement something and jump if equal		2 clocks

Therefore, in the scenario looks like loading of 65-128 bits of data will take 
12 clocks * 8 bits = 96 clocks. This will require 16 shift registers, instead of 12, 
but yields an effective data transfer rate of 0.75 clocks/bit
Now 256 updates/pixel look unrealistic, but 128 updates/pixel looks possible.
128 updates/pixel would mean that at 4 intensities we can blink the LED 8 times
even for the lowest intensity, which isn't so bad.

Assuming that data line + SHCP line can be throttled at the same time, 
we save only one clock cycle/bit, i.e., and lose 1 SR/bank, meaning that to load 57-112 bits of data
will take 11 clocks * 8 bits = 88 clocks (0.78 clocks/bit)

What if instead of using two banks we daisychain two SRs?  THen it will take 16 iterations of the loop 
to load the data, and even if each iteration is 10 cycles, it will take 160 clocks to load the data.
Therefore daisychaining SRs will result in longer loading times.

What, on the other hand, if we not use all the output bits on the SRs?  
In the "reliable" configuration, we have 16SRs, 
meaning that to achieve 90 bits we only need 6 of each 8 bits of the SR.
Each loop iteration now takes 12 clocks, but we only need 6 iterations, 
so we have 72 clock cycles/load.

We can sacrifice more bits on each SR, and add more SRs to gain more speed.
In this case we will need to add another bank.
This will add 4 clocks to each loop iteration, yielding 16 clocks/iteration.
At three banks, we can output 24 bits at a time. At 24 parallel bits, we only need
4 bits of each SR to control 90 bits.  At 16 clocks/iteration * 4 iteration we have 
64 clocks/load, and also we would need 24 shift registers :)

## Double-sided setup, cost, and current

ATmega328 has 28 total pins. 5 of these are power/REF pins, 1 is the RESET pin. 
2 are needed for bluetooth, and 1 for hall-effect sensor, leaving 19 GIO pins free.
This means we can comfortably have 2 banks of SRs.
But it gets worse if we wanted to do two lines of LEDs/side, (120 LEDs total = 360 data lines).
At minimum, this will require 45(!) shift registers. 
Using a banked setup will require 48 shift registers at minimum.
With 48 SRs you do have 384 bits of output, which means 128 leds, 
which works out to 32/spoke/side, or 192 mm of continuous coverage.

However, supporting 4 banks on one chip is not possible.
A single ATmega328 can comfortable support 2 banks without daisychaining.
ATmega16L has more IO lines (30 after reset/TX/RX and power), but even that 
isn't enough to support 4 banks. So, we'll probably have to daisychain 
or to have two ATmega328s who can talk to each other.

Crap!  Turns out that HC595 officially only allows 70mA max total output:
this works out to 8.75mA/LED if all LEDs are on.
SN74HC595, M74HC595, and CDXXHC4094 are the same or worse

One high-current shift register is TPIC6C595, 
These cost $2/piece on Ebay and $1/piece on Digikey  
(when you buy 50!)

Transistor array $10/$12/$13 for 50 on ebay
Digikey sells 7 darlington arrays for $30 / 100 ($15/50)

The LED driver chips, on the other hand, can sourcd 25mA on each pin, 
and 400mA total, which works out to 25mA/pin.  They are $2.50/chip. 
For a double-sided, double-spoked wheel, i.e. one with 120 leds (=360 bits) 
one would need 23 of these chips. They cost $51.60 on Digikey for 25 of these.

So, going with the shift register setup for double-sided double-spoked setup you pay
$10 / 200 SMD LEDs on ebay
$5  / 50 SMD CD74HC4094 on ebay
$10($13) / 50 th/smd darlington arrays on Ebay.
$5 bluetooth (ebay)
SMD resistors (cheap)
+PCB. Total cost: about $30

My bike: 240mm (=9.5") rim to center. A 1" wide PCB will be 10 square inches.  
It will cost $50/board (must be multiples of 3! from OSH Park) ouch.
If we stack 5mm leds together this is 48 leds.  
But they could be stacked towards the end for better text.

There is also WorldSemi (there's a reputable name!) WS2803 can drive 
18 bits and sells for $2/piece in quantities of 10, $
40 for 25 and $70/50 on ebay.
In a double spoke, double-sided setup 20 of these chips will be necessary.
The chip looks nice: it can provide constant current using only 1 resistor, 
provides 8-bit PWM capabilities, runs on any VCC up to 6V, 
can do up to 25 Mhz clock, and can source up to 30mA of current on eac pin.
There is a complicated formula for calculating the MAXIMUM I_OUT of the device 
that I have not worked through.
These chips having 8-bit pwm, require 144 bits of data to set the PWM parameters.
Assuming that the pallete is in SRAM while the image is in Flash, 
using bit-bangning techniques, and assuming PARALLEL LOADING, 
i.e. shared clock, and 8 different WS2803 on 8 different data lines (=48 total LEDs maximuj)
the inner loop of the data send should look like this:

	OUT 	# Lower clock, 1 cycle
	OUT 	# Set data for bit n, 1 cycle
	OUT 	# Raise clock, 1 cycle

This needs to happen for 144 bits, so this process will take 432 clocks, 
without even counting loading the data from flash. 
Even though WS2803 can be cascaded, given the fact that WS2803 has no latch pins, 
using cascaded WS2803 will result in skewed images.

Ebay also has TLC5940 chip, which is a 16-channel PWM led driver, 
this one having a latch. In quantities this chip goes for about $1/chip, 
in contrast to Digikey's LED drivers which are $2/chip.  
Even though TLC5940 is more expensive than WS2803, and, inconveniently has
16 output lines, rather than 18, it might be a better choice, 
since having a latch it can be cascaded for our purposes.  
Sparkfun users do not some weirdness about PWM-ing with these chips, 
but there is a book on it here:
https://sites.google.com/site/artcfox/demystifying-the-tlc5940
We would need 6 chips /side /spoke, or 24 chips for a two-sided double-spoked wheel.
As an additional benefit, using dot-correction mode, TLC5940
needs only 96 bits of data (6 bits/line), not 144 bits like the WS2803.
This means that given parallel TLC5940 chips, we have 42 LEDs/bank, 
and data send / bank will take 3*96=288 clock cycles (minimum)
Using cascaded chips means double the transmission time, i.e. 576 clocks/send.

With two dedicated send registers, four total banks (i.e. two chips daisychained)
(i.e. two sides, 2 spokes/side), 1 byte/pixel palletized image,
and only 15 channels/chip (for ease of xmission) 
we will have up to 40 pixels / side /spoke 
(8 chips/bank each having 15 output lines, i.e, 5 pixels).
This requires 32 chips, and correspondingly would cost roughly $32
Our load routine looks as follows then:

Loop A: for each cascade, repeat twice
	Loop B: loop pixels, repeats 5 times ()
		LPM				# Load program memory for bank A: one pixel. 3 cycles
		LPM				# Load program memory for bank B: one pixel. 3 cycles

		???				# Pallete lookup bank A. 1 cycle
		???				# Pallete lookup bank B. 1 cycle

		Loop C: for each color (repeats 3 times)
			Loop D: for each bit (repeats 6 times we are using 6 bit words)
				OUT			# Lower clock all banks, 1 cycle
				OUT			# Set data for bit n, 1 cycle
				OUT			# Raise clock, 1 cycle
				???			# Test and repeat, 2 cycles
				# loop D takes (3+2)*6=30 cycles

			???				# Advance colors for all banks (4 cycles)
			???				# Test and repeat (2 cycles)
			# Now we have sent 3 words to each bank.
			# Loop C takes (30+4+2)*3=108 cycles

	
		???					# Test and repeat (2 cycles)
		# Loop B takes (6+2+2+108)*5 = 590 

	???					# Test and repeat (2 cycles)
	# Loop A takes (590+2)*2 = 1184 cycles

Given that we have 16000 clock cycles between each pixel (@200 pixels @ 30mph), 
this should be sufficient.

Some people on Sparkfun recommend the TLC5952, but it isn't available on Ebay, 
and it costs $4 on Digikey.

## Bootloader

Bootloader is working. We may way to tweak the bootloader to load just the 
PROGMEM data, and not the whole program, but this is not critical right now.

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


#include "../shared/glcdfont.c"

#define nop()  __asm__ __volatile__("nop")





volatile	uint16_t	nHiResTimebaseCount;
volatile	uint16_t	nLastWheelTick;
volatile	uint16_t	nTicksPerRevolution;

volatile	uint32_t	nIntensityTimerHitCounter;

volatile	uint8_t	idxHorizontalPixel;

#define WHEEL_SPEED_TIMER_OCR			4
#define WHEEL_SPEED_TIMER_PRESCALER		256
#define WHEEL_SPEED_TIMER_PRESC_BITS	(1<<CS02)|(0<<CS01)|(0<<CS00)
#define WHEEL_SPEED_TIMER_FREQ_INT		(F_CPU/WHEEL_SPEED_TIMER_PRESCALER/WHEEL_SPEED_TIMER_OCR)
#define WHEEL_SPEED_TIMER_FREQ_FLOAT	((float)F_CPU/(float)WHEEL_SPEED_TIMER_PRESCALER/(float)WHEEL_SPEED_TIMER_OCR)

//#define INTENS_TIMER_PRESCALER			1
//#define INTENS_TIMER_PRESC_BITS			(0<<CS12)|(0<<CS11)|(1<<CS10)
//#define INTENS_TIMER_BASEFREQ_FLOAT		((float)F_CPU/(float)INTENS_TIMER_PRESCALER)
uint8_t		nIntensTimerPrescaler;

#if INTENSITY_LEVELS>1
volatile	uint8_t		idxIntensityTimeSlice;
#endif /* INTENSITY_LEVELS>1 */


#if INTENSITY_LEVELS>1
uint8_t intensityMap[INTENSITY_LEVELS] = { 0, 1, 3, 7 };
#endif 	

#ifndef PROGMEM_GRAPHIC
#endif /* PROGMEM_GRAPHIC */

#ifndef PROGMEM_GRAPHIC

#endif /* PROGMEM_GRAPHIC */

uint8_t checkUpdateWheelSpeed() {	/* Returns whether speed was updated */
	uint16_t nNewTickCount = nHiResTimebaseCount;
	if (nNewTickCount > 1000)	{		// This is a dumb-ass debouncing circuit
		nTicksPerRevolution = nNewTickCount;
		nHiResTimebaseCount = 0;
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
			// ... try at min prescaler
			uint32_t nCounterVal = round(F_CPU/fWheelFreq/(float)(INTENSITY_COUNTER_MAX)/(float)HORZ_PIXELS);
			if (nCounterVal > 0xFFFF) {
				// Must use a prescaler higher than one. Using 8
				nIntensTimerPrescaler = 8;
				TCCR1B = 0
					|(0<<CS12)|(1<<CS11)|(8<<CS10)			// Prescaler=1
					|(0<<WGM13)|(1<<WGM12)					// CTC Mode
					;

			} else {
				// Prescaler = 1 is sufficient
				nIntensTimerPrescaler = 1;
				TCCR1B = 0
					|(0<<CS12)|(0<<CS11)|(1<<CS10)			// Prescaler=1
					|(0<<WGM13)|(1<<WGM12)					// CTC Mode
					;
			}
			OCR1A = round(F_CPU/float(nIntensTimerPrescaler)/fWheelFreq/(float)(INTENSITY_COUNTER_MAX)/(float)HORZ_PIXELS);

#undef DPRINTBTH
#ifdef DPRINTBTH
//			dputsi("INTENSITY_COUNTER_MAX: ", INTENSITY_COUNTER_MAX);
			dputsl("nTicksPerRevolution: ", nTicksPerRevolution);
			dputsf("fWheelFreq: ", fWheelFreq, 2);
			dputsi("Prescaler: ", nIntensTimerPrescaler);
			dputsl("OCR1A: ", OCR1A);
#endif

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
		|(0<<CS12)|(0<<CS11)|(1<<CS10)		// Use prescaler one: useful for debugging
		//|(INTENS_TIMER_PRESC_BITS)			// Prescaler
		|(0<<WGM13)|(1<<WGM12)				// CTC Mode
		;
	OCR1A = 0xFFFF;							// Not doing anything until we get a wheel speed
	TIMSK1 = (1<<OCIE1A);					// Enable interrupt on timer1

	sei();									// Enable interrupts
	

	doDisplayInit();

	//~ uint8_t anData[4] = {0x00, 0x00, 0x00, 0x00};
	//~ sr.forceWriteData(0, 4, anData);	
	nTicksPerRevolution = 0;

	for (;;) {
		if (nIntensityTimerHitCounter != 0) {
			nIntensityTimerHitCounter = 0;
			doDisplayUpdate(
				idxHorizontalPixel
#				if INTENSITY_LEVELS>1			
				, idxIntensityTimeSlice
#				endif /* INTENSITY_LEVELS>1 */
				);
		}
	}

}