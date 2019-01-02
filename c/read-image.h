#ifndef _READIMAGE
#define _READIMAGE

#include <stdio.h>
#include <stdint.h>

void read_image_file(FILE* file, uint16_t memory[]);
int read_image(const char* image_path, uint16_t memory[]);

#endif