#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hash.h"
#include "lodepng.h"


void displayUsage() {
	printf("GENIMG: generate images");
	printf("Usage:\n\n");
	printf("GENIMG filename bitdepth [outfilename]");// [-padwidth desiredwidth padcolor]\n\n");
	printf("where bitdepth can be 2 or 3\n\n");
	printf("When bitdepth is two, the resulting image is palette-less:\n");
	printf("and each pixel occupies one byte.\n");
	printf("In this byte the high two bits are unused, \n");
	printf("while the rest are __RRGGBB\n\n");
	
	printf("At bitdepth 3, the image has a palette.\n");
	printf("Each pixel is still one byte, but now this byte\n");
	printf("is an index to a 256-color palette\n");
	printf("where each color is 3 bytes, i.e. 8-bits for each color channel.\n\n");
	
	//~ printf("When specifying -padwitdth, the padcolor should be expressed as RGB webcolor,");
	//~ printf("without the pound sign. For example FF0000 is red");
}

typedef struct palimage_t palimage_t;
struct palimage_t {
	unsigned char * palette;
	int palcount;
	unsigned char * pixdata;
	int w;
	int h;
};

void printArrayData(unsigned char * data, int w, int h, char * name, int width_div) {
	//~ printf("Print array data %d %d", w, h);
	char strWidth[10];
	sprintf(strWidth, "%d", w);
	if (width_div>1) {
		if (w%width_div) {
			fprintf(stderr, "ERROR: width_div specified to printArrayData, but width doesn't divide");
		} else {
			sprintf(strWidth, "%d * %d", w/width_div, width_div);
		}
	}
	if (h>1) {
		printf("uint8_t %s[%d][%s] = {\n", name, h, strWidth);
	} else {
		printf("uint8_t %s[%s] = {\n", name, strWidth);
	}
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
	printf("};\n");	
}

unsigned long makePalIndex(unsigned char * pixel, unsigned int intcount) {	
	unsigned long idx = 0;
	for (int idxChannel=0;idxChannel<3;idxChannel++) {
		// R = pixel[0], G = pixel[1], B = pixel[2], 
		unsigned short chan_int = 
			(unsigned short)
			round(
			((float)pixel[idxChannel]/255.0)
			*(float)(intcount-1));
		//~ printf("I:%d O:%d ", pixel[idxChannel], chan_int);
		idx += chan_int * pow(intcount, idxChannel);	// R=low, G=mid, B=high
	}
	//~ printf("\n");
	return idx;
}


palimage_t * palletizePNGdata(unsigned char * pngData, unsigned int w, unsigned int h, unsigned int intcount, unsigned char * forceColors, unsigned int forceCount) {
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

	// Set up the forced colors
	for (int idxForced = 0; idxForced<forceCount; idxForced++) {
		unsigned char * pixel = &(forceColors[idxForced*3]);
		unsigned long idx = makePalIndex(pixel, intcount);
		if (palette[idx]==0)
			palCount++;
		palette[idx] = 0xFF;
	}	

	for (int y=0; y<h; y++) {
		int x;
		for (x=0; x<w; x++) {
			unsigned char * pixel = &(pngData[(y*w+x)*3]);
			unsigned long idx = makePalIndex(pixel, intcount);
			//~ printf("Made color %#0.2x %#0.2x %#0.2x into palette index %#0.4x\n", pixel[0], pixel[1], pixel[2], idx);
			if (idx>palSize-1) {
				printf("Error: palette index too big\n");
				return NULL;
			}
			if (palette[idx] < 0xFF) {
				if (++palette[idx] == 1) {
					// First increase of THIS color
					palCount++;
				}
			}
		}
	}
	
	if (palCount>0xFF) {
		printf("Palette too large, and I don't know how to do color quantization\n");
		return NULL;
	} 
	
	
	// We have a 256-or less color palette
	// in the palette array each entry contains the number of times it is referenced.
	palimage_t * res = malloc(sizeof(palimage_t));
	res->palette = malloc(palCount * 3);
	res->palcount = palCount;
	res->pixdata = malloc(h*w*1);
	res->h = h;
	res->w = w;
	
	// Fix up the palette by assigning indexes to all the colors, and make the palette
	
	unsigned short idxNextPalIndex = forceCount;
	unsigned short idxNextForcedPalIndex = 0;
	unsigned short * pNextIndex;
	for (unsigned long idxPal = 0; idxPal<palSize; idxPal++) {
		if (palette[idxPal]) {					// Palette entry is used (=non-zero)
			unsigned char fForced = 0;
			for (int idxForced = 0; idxForced<forceCount; idxForced++) {
				unsigned char * pixel = &(forceColors[idxForced*3]);
				unsigned long idx = makePalIndex(pixel, intcount);
				if (idx==idxPal) {
					fForced = 1;
					break;
				}
			}
			pNextIndex = (fForced) ? &idxNextForcedPalIndex : &idxNextPalIndex;
			//~ printf("Making palette color out of index %d...", idxPal);
			palette[idxPal] = *pNextIndex;	// Assign the next palette index
			unsigned long ofs = (*pNextIndex)*3;
			(*pNextIndex)++;
			// Decode palette index into colors
			unsigned long val = idxPal;
			for (int idxChan = 2; idxChan>=0; idxChan--) {
				unsigned long digit = (long)pow(intcount, idxChan);
				res->palette[ofs + idxChan] = val/digit;
				val %= digit;
			}
			//~ printf("\n");
		}
	}
	
	for (int y=0; y<h; y++) {
		for (int x=0; x<w; x++) {
			unsigned char * pixel = &(pngData[(y*w+x)*3]);
			unsigned long idx = makePalIndex(pixel, intcount);
			res->pixdata[w*y+x] = palette[idx];
		}		
	}	
	
	free(palette);
	
	return res;
}

void saveAsPNG(palimage_t * img, const char* filename, unsigned int intcount) {
	//~ printf("Saving to PNG");
	// Create an output memory thing
	unsigned char * imgdata = malloc( (img->w) * (img->h) * 3 );
	unsigned char * t =imgdata;
	for (int y=0; y<img->h; y++) {
		for (int x=0; x<img->w; x++) {
			unsigned short palIdx = img->pixdata[y*(img->w)+x];
			for (int idxChan = 0; idxChan<3; idxChan++) {
				t[idxChan] = round(img->palette[palIdx*3+idxChan]/(float)(intcount-1)*255);
			}
			t+=3;
		}		
	}	
	//~ memset(imgdata, 0xff,  (img->w) * (img->h) * 3 );
	lodepng_encode_file(filename, imgdata, img->w, img->h, LCT_RGB, 8);	
	free(imgdata);
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
	// Get output filename
	char * strOutFile = NULL;
	if (argc>3) { 
		strOutFile = argv[3];
	}
	
	lodepng_decode_file(&pngData, &w, &h, strFilename, LCT_RGB, 8);
	
	// Palletize the file
	unsigned char forcedColors[3] = {0, 0, 0};
	palimage_t * newdata = palletizePNGdata(pngData, w, h, (1<<nBitDepth), forcedColors, 1);
	int retval = 0;
	if (newdata) {
		// Have valid palette and data
		
		if (strOutFile) {
			// Save to file
			saveAsPNG(newdata, strOutFile, (1<<nBitDepth));
		}
		
		// Do padding here
		
		if (nBitDepth > 2) {
			// Print the palette data
			printArrayData(newdata->palette, newdata->palcount*3, 1, "palette", 3);
		} else {
			// Collapse the palette and the pixel data: 
			// in 2-bit color we are not using a palette, just storing the color data directly
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
		printArrayData(newdata->pixdata, w, h, "graphic", 1);
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