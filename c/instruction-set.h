#ifndef _INSTRUCTION_SET
#define _INSTRUCTION_SET

#include <stdint.h>

/* Instruction Set and Opcodes */
enum {
  OP_BR = 0,  /* Branch */
  OP_ADD,     /* Add */
  OP_LD,      /* Load */
  OP_ST,      /* Store */
  OP_JSR,     /* Jump to Subroutine */
  OP_AND,     /* Bitwise AND */
  OP_LDR,     /* Load Register */
  OP_STR,     /* Store Register */
  OP_RTI,     /* Return from Interrupt (unused) */
  OP_NOT,     /* Bitwise NOT */
  OP_LDI,     /* Load Indirect */
  OP_STI,     /* Store Indirect */
  OP_JMP,     /* Jump (also RET return) */
  OP_RES,     /* Reserved (unused) */
  OP_LEA,     /* Load Effective Address */
  OP_TRAP     /* Execute Trap */
};

/* Trap codes */
enum {
  TRAP_GETC = 0x20,   /* Get a character from the keyboard */
  TRAP_OUT = 0x21,    /* Output a character */
  TRAP_PUTS = 0x22,   /* Output a word string */
  TRAP_IN = 0x23,     /* Input a string */
  TRAP_PUTSP = 0x24,  /* Output a byte string */
  TRAP_HALT = 0x25    /* Halt the program */
};

void add(uint16_t instruction);
void and(uint16_t instruction);
void branch(uint16_t instruction);
void jump(uint16_t instruction);
void jumpToSubroutine(uint16_t instruction);
void load(uint16_t instruction);
void loadIndirect(uint16_t instruction);
void loadRegister(uint16_t instruction);
void loadEffectiveAddress(uint16_t instruction);
void not(uint16_t instruction);
void store(uint16_t instruction);
void storeIndirect(uint16_t instruction);
void storeRegister(uint16_t instruction);
void trapGetC();
void trapHalt();
void trapIn();
void trapOut();
void trapPuts();
void trapPutSP();
void trap(uint16_t instruction);

#endif