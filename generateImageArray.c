#include "lodepng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	unsigned char * pngData;
	unsigned int h, w;
	lodepng_decode_file(&pngData, &h, &w, "ghost1.png", LCT_RGB, 8);
	int y;
	printf("{\n");
	for (y=0; y<h; y++) {
		printf("{ ");
		int x;
		for (x=0; x<w; x++) {
			unsigned char bd = 
				(pngData[(y*w+x)*3] >> 6) 	<< 4		// R
			+
				(pngData[(y*w+x)*3+1] >> 6)	<< 2		// G
			+
				(pngData[(y*w+x)*3+2] >> 6)	<< 0		// B
				;
			if (bd==0) {
				printf("0x00");
			} else {
				printf("%#0.2x", bd);
			}
			if (x<w-1) 
				printf(", ");
		}
		if (y<h-1) {
			printf("}, \n");
		} else {
			printf("}\n");
		}
	}
	printf("}\n");
}