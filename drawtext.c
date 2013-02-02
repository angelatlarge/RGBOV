#include <string.h>
#include "glcdfont.c"

#include <stdint.h>


//~ void drawText(uint8_t pMemory[VERTICAL_PIXELS][HORZ_PIXELS], uint8_t w, uint8_t h, char * str, uint8_t bg, uint8_t fg, int16_t x, int16_t y) {
void eraseGraphic(uint8_t * graphic, uint16_t w, uint16_t h) {
	memset(&graphic[0], 00, sizeof(uint8_t)*w*h);
}


uint16_t drawText(uint8_t* graphic, uint16_t w, uint16_t h, const char * str, uint8_t bg, uint8_t fg, int16_t x, int16_t y) {
	uint8_t idxChar = 0;
	while (str[idxChar]) {
		uint8_t i;
		for (i=0; i<6; i++ ) {	// X LOOP i is the x dimension in the font
			uint8_t line;
			if (i == 5) 
				line = 0x0;
			else {
#				ifdef __AVR
				line = pgm_read_byte(font+(str[idxChar]*5)+i);
#				else
				line = font[(str[idxChar]*5)+i];
#				endif				
			}
			
			uint8_t j;
			for (j = 0; j<8; j++) {	// Y loop
				if ((x+i<w) && (y+j<h)) {
					//~ graphic[y+j][x+i] = (line & 0x1) ? fg : bg;
					graphic[(y+j)*w+(x+i)] = (line & 0x1) ? fg : bg;
				}
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