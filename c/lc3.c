/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#include "../core/bit-utilities.h"
#include "../core/core.h"
#include "../core/input-buffering.h"
#include "../core/read-image.h"

#include "instruction-set.h"

// Standard fetch/execute cycle using switch statement
void fetchExecute() {
  /* FETCH */
  uint16_t instruction = mem_read(registers[R_PC]++);
  uint16_t opcode = instruction >> 12;

  switch (opcode) {
  case OP_ADD:
    add(instruction);      
    break;
  case OP_AND:
    and(instruction);
    break;
  case OP_NOT:
    not(instruction);
    break;
  case OP_BR:
    branch(instruction);
    break;
  case OP_JMP:
    jump(instruction);
    break;
  case OP_JSR:
    jumpToSubroutine(instruction);
    break;
  case OP_LD:
    load(instruction);
    break;
  case OP_LDI:
    loadIndirect(instruction);
    break;
  case OP_LDR:
    loadRegister(instruction);
    break;
  case OP_LEA:
    loadEffectiveAddress(instruction);
    break;
  case OP_ST:
    store(instruction);
    break;
  case OP_STI:
    storeIndirect(instruction);
    break;
  case OP_STR:
    storeRegister(instruction);
    break;
  case OP_TRAP:
    trap(instruction);
    break;
  case OP_RES:
    abort();
    break;
  case OP_RTI:
    abort();
    break;
  default:
    // Bad opcode
    printf("BAD OPCODE\n");
    break;
  }
}

// Alternate fetch/execute using computed GOTO
// This method supposedly uses less branching by
// eliminating the outer while loop
// Each instruction should use only one JMP instruction
// instead of two, which should make the execution faster
// However, compiler optimizations may make the 
// execution times of both approaches approximately the same.
// I have not personally instrumented the code to determine
// if the computed GOTO method offers any advantages
// See: https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
// Also: https://news.ycombinator.com/item?id=18678699
#define DISPATCH() {\
  currentInstruction = mem_read(registers[R_PC]++);\
  uint16_t opcode = currentInstruction >> 12;\
  goto *dispatch_table[opcode];\
}

void fetchExecuteComputedGoto() {

  // NOTE: THE ORDER OF THIS TABLE
  // MUST MATCH THE ORDER OF THE INSTRUCTIONS
  // IN instruction-set.h
  // i.e. OP_BR must be at index 0 etc.
  static void *dispatch_table[] = {
    &&OP_BR, 
    &&OP_ADD, 
    &&OP_LD, 
    &&OP_ST,
    &&OP_JSR, 
    &&OP_AND, 
    &&OP_LDR, 
    &&OP_STR,
    &&OP_RTI, 
    &&OP_NOT, 
    &&OP_LDI, 
    &&OP_STI, 
    &&OP_JMP, 
    &&OP_RES, 
    &&OP_LEA, 
    &&OP_TRAP
  };

  uint16_t currentInstruction;

  DISPATCH();

  OP_ADD:
    add(currentInstruction);
    DISPATCH();
  OP_AND:
    and(currentInstruction);
    DISPATCH();
  OP_NOT:
    not(currentInstruction);
    DISPATCH();
  OP_BR:
    branch(currentInstruction);
    DISPATCH();
  OP_JMP:
    jump(currentInstruction);
    DISPATCH();
  OP_JSR:
    jumpToSubroutine(currentInstruction);
    DISPATCH();
  OP_LD:
    load(currentInstruction);
    DISPATCH();
  OP_LDI:
    loadIndirect(currentInstruction);
    DISPATCH();
  OP_LDR:
    loadRegister(currentInstruction);
    DISPATCH();
  OP_LEA:
    loadEffectiveAddress(currentInstruction);
    DISPATCH();
  OP_ST:
    store(currentInstruction);
    DISPATCH();
  OP_STI:
    storeIndirect(currentInstruction);
    DISPATCH();
  OP_STR:
    storeRegister(currentInstruction);
    DISPATCH();
  OP_TRAP:
    trap(currentInstruction);
    DISPATCH();
  OP_RES:
    abort();
    DISPATCH();
  OP_RTI:
    abort();
    DISPATCH();
}

/* MAIN */
int main(int argc, const char* argv[]) {

  if (argc < 2) {
    /* show usage string */
    printf("lc3 [image-file1] ...\n");
    exit(2);
  }

  for (int j = 1; j < argc; ++j) {
    if (!read_image(argv[j], memory)) {
      printf("failed to load image: %s\n", argv[j]);
      exit(1);
    }
  }

  signal(SIGINT, handle_interrupt);
  disable_input_buffering();

  /* Set the Program Counter to the default address:
  0x3000
  
  Lower addresses are left empty to leave space 
  for trap routines
  */
  enum { PC_START = 0x3000 };
  registers[R_PC] = PC_START;

  // Fetch/Execute using switch statements
  /*
  while (1) {
    fetchExecute();
  }// end while
  //*/

  // Fetch/Execute using computed GOTO
  fetchExecuteComputedGoto();

  restore_input_buffering();
}