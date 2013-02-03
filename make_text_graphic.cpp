#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drawtext.h"

#define	HORZ_PIXELS				180
#define VERTICAL_PIXELS			10

int main(int argc, char *argv[]) {
	int h = HORZ_PIXELS;
	int v = VERTICAL_PIXELS;
	int fg = 0xFF;
	int bg = 0x00;
	char strSampleText[] = "The quick brown fox jumped over the lazy dog";
	char * strText = strSampleText;
	if (argc>0) {
		//~ printf(argv[1]);
		strText = argv[1];
	} 
	if (argc>1) { h = atol(argv[2]);}
	if (argc>2) { v = atol(argv[3]);}
	if (argc>3) { bg = atol(argv[4]);}
	if (argc>4) { fg = atol(argv[5]);}
	//~ printf("%s %d %d", strText, h, v);
	uint8_t* graphic = (uint8_t*)malloc(sizeof(uint8_t)*h*v);
	memset(graphic, bg, sizeof(uint8_t)*h*v);
	drawText(graphic, h, v, strText, bg, fg, 0, 0);
	printf("{\n");
	for (int y=0; y<v; y++) {
		printf("{");
		for (int x=0; x<h; x++) {
			uint8_t byte = graphic[y*h+x];
			if (byte==0) {
				printf("0x00");
			} else {
				printf("%#0.2x", byte);
			}
			if (x<(h-1)) {
				printf(", ");
			}
		}
		if (y<v-1) {
			printf("},\n");
		} else {
			printf("}\n");
		}
		
	}
	printf("};\n");
	free(graphic);
}