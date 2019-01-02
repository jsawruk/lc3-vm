#ifndef _CORE
#define _CORE

#include <stdint.h>

/* Memory: 65536 bytes */
uint16_t memory[UINT16_MAX];

/* Registers
R0 - R7: General purpose
PC: Program Counter
COND: Condition Flags
*/
enum {
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC, /* Program Counter */
  R_COND,
  R_COUNT
};
uint16_t registers[R_COUNT];

/* Memory Mapped Registers */
enum {
  MR_KBSR = 0xFE00, /* keyboard status */
  MR_KBDR = 0xFE02  /* keyboard data */
};

/* Condition Flags */
enum {
  FL_POS = 1 << 0, /* P(ositive) */
  FL_ZRO = 1 << 1, /* Z(ero) */
  FL_NEG = 1 << 2  /* N(egative) */
};

void update_flags(uint16_t r);

void mem_write(uint16_t address, uint16_t val);
uint16_t mem_read(uint16_t address);

#endif