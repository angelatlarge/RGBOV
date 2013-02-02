#ifndef DRAWTEXT_H
#define DRAWTEXT_H

#include <stdint.h>

/*
#ifndef __AVR
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef int int16_t;
#endif
*/
extern "C"
{
uint16_t drawText(uint8_t* graphic, uint16_t w, uint16_t h, const char * str, uint8_t bg, uint8_t fg, int16_t x, int16_t y);
void eraseGraphic(uint8_t* graphic, uint16_t w, uint16_t h);
}

#endif /* DRAWTEXT_H */
