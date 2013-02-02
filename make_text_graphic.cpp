#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drawtext.h"

#define	HORZ_PIXELS				180
#define VERTICAL_PIXELS			10


int main() {
	uint8_t* graphic = (uint8_t*)malloc(sizeof(uint8_t)*HORZ_PIXELS*VERTICAL_PIXELS);
	drawText(graphic, HORZ_PIXELS, VERTICAL_PIXELS, "We hold these truths", 0x00, 0xFF, 0, 0);
	printf("{\n");
	for (int y=0; y<VERTICAL_PIXELS; y++) {
		printf("{");
		for (int x=0; x<HORZ_PIXELS; x++) {
			uint8_t byte = graphic[y*HORZ_PIXELS+x];
			if (byte==0) {
				printf("0x00");
			} else {
				printf("%#.2x", byte);
			}
			if (x<(HORZ_PIXELS-1)) {
				printf(", ");
			}
		}
		if (y<VERTICAL_PIXELS-1) {
			printf("},\n");
		} else {
			printf("}\n");
		}
		
	}
	printf("};\n");
	free(graphic);
}