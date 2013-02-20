#ifndef LOAD_H
#define LOAD_H

#include <avr/io.h>

/* 	Prepares and/or sends the next pixels, but does not activate them.
	Guaranteed to be called once before loadingUpdateDisplay() is called
*/
void loadingPrepareUpdate(
	uint8_t idxHorizontalPixel
#	if INTENSITY_LEVELS>1			
	, uint8_t idxIntensityTimeSlice
#	endif /* INTENSITY_LEVELS>1 */
	);

/* Activates the pixels for the current column */
void loadingUpdateDisplay(
	uint8_t idxHorizontalPixel
#	if INTENSITY_LEVELS>1			
	, uint8_t idxIntensityTimeSlice
#	endif /* INTENSITY_LEVELS>1 */
	);
	
/* Initialization function */	
void loadingInitDisplay();

#endif /* LOAD_H */
