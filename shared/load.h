#ifndef LOAD_H
#define LOAD_H

#include <avr/io.h>


void doDisplayUpdate(
	uint8_t idxHorizontalPixel
#	if INTENSITY_LEVELS>1			
	, uint8_t idxIntensityTimeSlice
#	endif /* INTENSITY_LEVELS>1 */
	);
void doDisplayInit();

#endif /* LOAD_H */
