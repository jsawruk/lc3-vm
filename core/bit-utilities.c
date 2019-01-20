#include <stdint.h>
#include "bit-utilities.h"

// Convert Big Endian to little endian
uint16_t swap16(uint16_t x) {

    return (x << 8) | (x >> 8);
}

// Sign extend a two's complement number to 16 bits
uint16_t sign_extend(uint16_t x, int bit_count) {
  
  if ((x >> (bit_count - 1)) & 1) {
      x |= (0xFFFF << bit_count);
  }
  return x;
}