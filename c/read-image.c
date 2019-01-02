#include <stdio.h>
#include <stdint.h>

#include "read-image.h"

// Read an executable file into memory
void read_image_file(FILE* file, uint16_t memory[]) {

  printf("READ IMAGE FILE\n");

  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);

  // NOTE: LC-3 is Big Endian, but x86-64 is little endian
  origin = swap16(origin);

  uint16_t max_read = UINT16_MAX - origin;
  uint16_t* program = memory + origin;
  size_t read = fread(program, sizeof(uint16_t), max_read, file);

  /* Convert program from Big Endian to little endian */
  while (read-- > 0){
    *program = swap16(*program);
    ++program;
  }
}

// Given a path, load the program into memory
int read_image(const char* image_path, uint16_t memory[]) {

  printf("READ IMAGE\n");
  
  FILE* file = fopen(image_path, "rb");
  
  if (!file) { 
    return 0; 
  };
  
  read_image_file(file, memory);
  fclose(file);
  return 1;
}