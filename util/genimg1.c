#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hash.h"
#include "lodepng.h"


void displayUsage() {
	printf("GENIMG: generate images");
	printf("Usage:\n\n");
	printf("GENIMG filename bitdepth [outfilename]\n\n");
	printf("where bitdepth can be 2 or 3\n\n");
	printf("When bitdepth is two, the resulting image is palette-less:\n");
	printf("and each pixel occupies one byte.\n");
	printf("In this byte the high two bits are unused, \n");
	printf("while the rest are __RRGGBB\n\n");
	
	printf("At bitdepth 3, the image has a palette.\n");
	printf("Each pixel is still one byte, but now this byte\n");
	printf("is an index to a 256-color palette\n");
	printf("where each color is 3 bytes, i.e. 8-bits for each color channel.");
}

typedef struct palimage_t palimage_t;
struct palimage_t {
	unsigned char * palette;
	int palcount;
	unsigned char * pixdata;
	int w;
	int h;
};

void printArrayData(unsigned char * data, int w, int h) {
	printf("Print array data %d %d", w, h);
	printf("{\n");
	for (int y=0; y<h; y++) {
		// Open brace if more than one row
		if (h>1) { printf("{ "); }
		
		// Row data
		for (int x=0; x<w; x++) {
			unsigned char b = data[y*w+x];
			if (b==0) {
				printf("0x00");
			} else {
				printf("%#0.2x", b);
			}
			if (x<w-1) 
				printf(", ");
		}
		// Close brace if more than one row
		if (h>1) {
			if (y<h-1) {
				printf("}, \n");
			} else {
				printf("}\n");
			}
		}
	}
	printf("}\n");	
}

unsigned long makePalIndex(unsigned char * pixel, unsigned int intcount) {	
	unsigned long idx = 0;
	for (int idxChannel=0;idxChannel<3;idxChannel++) {
		// R = pixel[0], G = pixel[1], B = pixel[2], 
		unsigned short chan_int = 
			(unsigned short)
			round(
			((float)pixel[idxChannel]/255.0)
			//~ *(float)intcount);
			*(float)(intcount-1));
		//~ printf("I:%d O:%d ", pixel[idxChannel], chan_int);
		idx += chan_int * pow(intcount, idxChannel);	// R=low, G=mid, B=high
	}
	//~ printf("\n");
	return idx;
}


palimage_t * palletizePNGdata(unsigned char * pngData, unsigned int w, unsigned int h, unsigned int intcount) {
	/* 
		1. Build a list of all the colors used in the image, while reducing them to our predefined intensities
			At 8 intensities, we can have 8^3=512 maximum colors
			In 8-bits/channel RGB color space, the theoretical number of colors is 17million, 
			however, in reality this is limited by image size: in 500x100 image there can only be 50K colors
	*/
	unsigned long palSize = intcount*intcount*intcount;
	unsigned char * palette = malloc(palSize);
	unsigned long palCount = 0;
	memset(&palette[0], 0, palSize);
	int y;
	for (y=0; y<h; y++) {
		int x;
		for (x=0; x<w; x++) {
			unsigned char * pixel = &(pngData[(y*w+x)*3]);
			//~ unsigned char chan[3] = 
			//~ {pngData[(y*w+x)*3+0], pngData[(y*w+x)*3+1], pngData[(y*w+x)*3+2]};
			unsigned long idx = makePalIndex(pixel, intcount);
			printf("Made color %#0.2x %#0.2x %#0.2x into palette index %#0.4x\n", pixel[0], pixel[1], pixel[2], idx);
			if (idx>palSize-1) {
				printf("Error: palette index too big\n");
				return NULL;
			}
			if (++palette[idx] == 1) {
				// First increase of THIS color
				palCount++;
			}
		}
	}
	
	if (palCount>0xFF) {
		printf("Palette too large, and I don't know how to do color quantization\n");
		return NULL;
	} 
	
	
	// We have a 256-or less color palette
	palimage_t * res = malloc(sizeof(palimage_t));
	res->palette = malloc(palCount * 3);
	res->palcount = palCount;
	res->pixdata = malloc(h*w*1);
	res->h = h;
	res->w = w;
	
	// Fix up the palette by assigning indexes to all the colors, and make the palette
	//~ res->palette[0] = 0;				// First color = BLACK
	unsigned short idxNextPalIndex = 0;
	for (unsigned long idxPal = 0; idxPal<palSize; idxPal++) {
		if (palette[idxPal]) {					// Palette entry is used (=non-zero)
			printf("Making palette color out of index %d...", idxPal);
			palette[idxPal] = idxNextPalIndex;	// Assign the next palette index
			unsigned long ofs = idxNextPalIndex*3;
			idxNextPalIndex++;
			// Decode palette index into colors
			unsigned long val = idxPal;
			for (int idxChan = 2; idxChan>=0; idxChan--) {
				unsigned long digit = (long)pow(intcount, idxChan);
				res->palette[ofs + idxChan] = val/digit;
				val %= digit;
				printf("%#0.2x ", res->palette[ofs + idxChan]);
			}
			printf("\n");
		}
	}
	
	
	for (y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			unsigned char * pixel = &(pngData[(y*w+x)*3]);
			unsigned long idx = makePalIndex(pixel, intcount);
			res->pixdata[w*y+x] = palette[idx];
		}		
	}	
	
	free(palette);
	
	return res;
}




int main(int argc, char *argv[]) {
	unsigned char * pngData;
	unsigned int h, w;

	// Get the filename
	if (argc<2) { displayUsage(); return 1; }
	char* strFilename  = argv[1];
	// Get the bit depth
	if (argc<3) { displayUsage(); return 1; }
	int nBitDepth = atoi(argv[2]);
	switch (nBitDepth) {
		case 2: break;
		case 3: break;
		default: {
			printf("Invalid bitdepth specified\n");
			return 2;
		}
	}
	
	lodepng_decode_file(&pngData, &w, &h, strFilename, LCT_RGB, 8);
	
	// Palletize the file
	palimage_t * newdata = palletizePNGdata(pngData, w, h, (1<<nBitDepth));
	int retval = 0;
	if (newdata) {
		
		// Have valid palette and data
		//~ if (1) {
		if (nBitDepth > 2) {
			// Print the palette data
			printArrayData(newdata->palette, newdata->palcount*3, 1);
		} else {
			// Collapse the palette and the pixel data
			for (int y=0; y<h; y++) {
				for (int x=0; x<w; x++) {
					unsigned char * pal = &(newdata->palette[newdata->pixdata[(w*y+x)]*3]);
					newdata->pixdata[w*y+x] = 
							pal[0]<<4
						|	pal[1]<<2
						|	pal[2]<<0
						;
				}
			}
		}
		
		// Print the pixel and palette data
		printArrayData(newdata->pixdata, w, h);
	} else {
		// Palettization failed
		retval = 1;
	}
	if (newdata) {
		if (newdata->pixdata) { free(newdata->pixdata); newdata->pixdata = NULL; }
		if (newdata->palette) { free(newdata->palette); newdata->palette = NULL; }
		free(newdata);
		newdata = NULL;
	}
}