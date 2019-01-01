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

int running;

/* Struct for input buffering */
struct termios original_tio;

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

/* Condition Flags */
enum {
  FL_POS = 1 << 0, /* P(ositive) */
  FL_ZRO = 1 << 1, /* Z(ero) */
  FL_NEG = 1 << 2  /* N(egative) */
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

/* INPUT BUFFERING */
void disable_input_buffering() {

  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal) {
  restore_input_buffering();
  printf("\n");
  exit(-2);
}

/* UTILITIES */
uint16_t check_key() {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

// Convert Big Endian to little endian
uint16_t swap16(uint16_t x) {

    return (x << 8) | (x >> 8);
}

// Read an executable file into memory
void read_image_file(FILE* file) {

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
int read_image(const char* image_path) {

  printf("READ IMAGE\n");
  
  FILE* file = fopen(image_path, "rb");
  
  if (!file) { 
    return 0; 
  };
  
  read_image_file(file);
  fclose(file);
  return 1;
}

// Sign extend a two's complement number to 16 bits
uint16_t sign_extend(uint16_t x, int bit_count) {
  
  if ((x >> (bit_count - 1)) & 1) {
      x |= (0xFFFF << bit_count);
  }
  return x;
}

void update_flags(uint16_t r) {
  if (registers[r] == 0)
  {
      registers[R_COND] = FL_ZRO;
  }
  else if (registers[r] >> 15) /* a 1 in the left-most bit indicates negative */
  {
      registers[R_COND] = FL_NEG;
  }
  else
  {
      registers[R_COND] = FL_POS;
  }
}

/* MEMORY ACCESS */
void mem_write(uint16_t address, uint16_t val) {
    memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
  
  if (address == MR_KBSR) {
    if (check_key()) {
        memory[MR_KBSR] = (1 << 15);
        memory[MR_KBDR] = getchar();
    }
    else {
        memory[MR_KBSR] = 0;
    }
  }
  return memory[address];
}

/* INSTRUCTIONS */
void add(uint16_t instruction) {

  /* Instruction format:
    Register mode (Mode bit 0):

    15          Dest    Src1   Mode       Src2  0
    |-------------------------------------------|
    | 0 0 0 1 | D D D | A A A | 0 | 0 0 | B B B |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    A A A = 3-bit Source 1 Register
    B B B = 3-bit Source 2 Register

    Immediate mode (Mode bit 1):

    15          Dest    Src1  Mode  Immediate   0
    |-------------------------------------------|
    | 0 0 0 1 | D D D | A A A | 1 | I I I I I   |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    A A A = 3-bit Source 1 Register
    I I I I I = 5-bit Immediate Value Two's Complement Integer

    NOTE: The immediate value must be sign extended
  */

  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get the source 1 register
  uint16_t sourceRegister1 = (instruction >> 6) & 0x7;

  // Get the immediate mode flag
  uint16_t immediateFlag = (instruction >> 5) & 0x1;

  if (immediateFlag) {
    // Sign extend the immediate value
    uint16_t immediateValue = instruction & 0x1F;
    uint16_t signExtendedImmediateValue = sign_extend(immediateValue, 5);
    registers[destination] = registers[sourceRegister1] + signExtendedImmediateValue;
  }
  else {
    uint16_t sourceRegister2 = instruction & 0x7;
    registers[destination] = registers[sourceRegister1] + registers[sourceRegister2];
  }

  // Update the flags
  update_flags(destination);
}

void and(uint16_t instruction) {

  /* Instruction format:
  
  Register mode (Mode bit 0):

    15          Dest    Src1   Mode       Src2  0
    |-------------------------------------------|
    | 0 1 0 1 | D D D | A A A | 0 | 0 0 | B B B |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    A A A = 3-bit Source 1 Register
    B B B = 3-bit Source 2 Register

  Immediate mode (Mode bit 1):

    15          Dest    Src1  Mode  Immediate   0
    |-------------------------------------------|
    | 0 1 0 1 | D D D | A A A | 1 | I I I I I   |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    A A A = 3-bit Source 1 Register
    I I I I I = 5-bit Immediate Value Two's Complement Integer

    NOTE: The immediate value must be sign extended
  */

  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get the source 1 register
  uint16_t sourceRegister1 = (instruction >> 6) & 0x7;

  // Get the immediate mode flag
  uint16_t immediateFlag = (instruction >> 5) & 0x1;

  if (immediateFlag) {
    // Sign extend the immediate value
    uint16_t immediateValue = instruction & 0x1F;
    uint16_t signExtendedImmediateValue = sign_extend(immediateValue, 5);
    registers[destination] = registers[sourceRegister1] & signExtendedImmediateValue;
  }
  else {
    uint16_t sourceRegister2 = instruction & 0x7;
    registers[destination] = registers[sourceRegister1] & registers[sourceRegister2];
  }
}

void branch(uint16_t instruction) {

  /* Instruction Format:
    15          Flags   PCOffset9               0
    |-------------------------------------------|
    | 0 0 0 0 | N Z P | P P P P P P P P P       |
    |-------------------------------------------|
    N = Negative Flag (BRN)
    Z = Zero Flag (BRZ)
    P = Positive Flag (BRP)
    P P P P P P P P P = PCOffset9

    Flags can be combined to produce additional branch opcodes:
    BRZP
    BRNP
    BRNZ
    BRNZP (also equal to BR)

    Sign extend PCOffset9 and add to PC.
  */
  
  // Get PCOffset9
  uint16_t pcOffset9 = instruction & 0x1FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset9, 9);

  // Get the flags
  uint16_t conditionalFlags = (instruction >> 9) & 0x7;
  if (conditionalFlags & registers[R_COND]) {
    // If the branch conditions are met, branch
    registers[R_PC] += signExtendedPCOffset;
  }
}

void jump(uint16_t instruction) {

  /* Instruction Format:
  JMP mode:

    15                 Register                 0
    |-------------------------------------------|
    | 1 1 0 0 | 0 0 0 | R R R | 0 0 0 0 0 0     |
    |-------------------------------------------|
    R R R = 3-bit base register

  RET mode:

    15                                          0
    |-------------------------------------------|
    | 1 1 0 0 | 0 0 0 | 1 1 1 | 0 0 0 0 0 0     |
    |-------------------------------------------|
    
    NOTE: RET always loads R7
  */

  // Get the base register
  uint16_t baseRegister = (instruction >> 6) & 0x7;
  registers[R_PC] = registers[baseRegister];
}

void jumpToSubroutine(uint16_t instruction) {

  /* Instruction Format:
  JSR mode:

    15             PCOffset11                   0
    |-------------------------------------------|
    | 0 1 0 0 | 1 | P P P | P P P | P P P | P P |
    |-------------------------------------------|
    P P P P P P P P P P P = PCOffset11

  JSRR mode:

    15                   Register               0
    |-------------------------------------------|
    | 0 1 0 0 | 0 | 0 0 | R R R | 0 0 0 0 0 0   |
    |-------------------------------------------|
    R R R = 3-bit base register
  */

  // Get the base register
  uint16_t baseRegister = (instruction >> 6) & 0x7;
  uint16_t pcOffset11 = instruction & 0x7FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset11, 11);
  uint16_t longFlag = (instruction >> 11) & 1;

  // Store the current PC value into R7
  registers[R_R7] = registers[R_PC];

  if (longFlag) {
    // JSR
    registers[R_PC] += signExtendedPCOffset;
  }
  else {
    // JSRR
    registers[R_PC] = registers[baseRegister];
  }
}

void load(uint16_t instruction) {

  /* Instruction Format:
    15          Dest   PCOffset9                0
    |-------------------------------------------|
    | 0 0 1 0 | D D D | P P P P P P P P P       |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    P P P P P P P P P = PCOffset9

    Sign extend PCOffset9 and add to PC.
    Load the value at that memory address into destination
  */

  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get PCOffset9
  uint16_t pcOffset9 = instruction & 0x1FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset9, 9);

  uint16_t value = mem_read(registers[R_PC] + signExtendedPCOffset);
  registers[destination] = value;

  // Update the flags
  update_flags(destination);
}

void loadIndirect(uint16_t instruction) {
  
  /* Instruction Format:
    15          Dest   PCOffset9                0
    |-------------------------------------------|
    | 1 0 1 0 | D D D | P P P P P P P P P       |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    P P P P P P P P P = PCOffset9

    Sign extend PCOffset9 and add to PC. The value
    stored at that memory location (A) is another address (B). 
    The value stored in memory location B is loaded
    into the destination register.
    (A points to B. The value is located at memory location B)
  */

  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get PCOffset9
  uint16_t pcOffset9 = instruction & 0x1FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset9, 9);

  // Add the current PC value
  uint16_t pointerLocation = registers[R_PC] + signExtendedPCOffset;

  // Read the pointer
  uint16_t pointer = mem_read(pointerLocation);

  // Read the value referred to by the pointer
  uint16_t value = mem_read(pointer);

  // Write the value to the register
  registers[destination] = value;

  // Update the flags
  update_flags(destination);
}

void loadRegister(uint16_t instruction) {

  /* Instruction Format:
    15          Dest   Base     Offset6         0
    |-------------------------------------------|
    | 0 1 1 0 | D D D | B B B | O O O O O O     |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    B B B = 3-bit Base Register
    O O O O O O = 6-bit offset

    Sign extend the offset, add this value to
    the value in the base register. Read the 
    memory at this location and load into
    destination
  */
  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get the base register
  uint16_t baseRegister = (instruction >> 6) & 0x7;

  // Get the offset
  uint16_t offset = instruction & 0x3F;
  uint16_t signExtendedOffset = sign_extend(offset, 6);

  uint16_t value = mem_read(registers[baseRegister] + signExtendedOffset);

  registers[destination] = value;

  // Update the flags
  update_flags(destination);
}

void loadEffectiveAddress(uint16_t instruction) {

  /* Instruction Format:
    15          Dest   PCOffset9                0
    |-------------------------------------------|
    | 1 1 1 0 | D D D | P P P P P P P P P       |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    P P P P P P P P P = PCOffset9

    Sign extend PCOffset9, add to PC, and store
    that ADDRESS in the destination register
  */

  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get PCOffset9
  uint16_t pcOffset9 = instruction & 0x1FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset9, 9);

  registers[destination] = registers[R_PC] + signExtendedPCOffset;

  // Update the flags
  update_flags(destination);
}

void not(uint16_t instruction) {

  /* Instruction Format:
    15          Dest    Src    Mode             0
    |-------------------------------------------|
    | 1 0 0 1 | D D D | S S S | 1 | 1 1 1 1 1   |
    |-------------------------------------------|
    D D D = 3-bit Destination Register
    S S A = 3-bit Source Register
  */

  // Get the destination register
  uint16_t destination = (instruction >> 9) & 0x7;

  // Get the source 1 register
  uint16_t sourceRegister = (instruction >> 6) & 0x7;

  registers[destination] = ~registers[sourceRegister];

  // Update the flags
  update_flags(destination);
}

void store(uint16_t instruction) {

  /* Instruction Format:
    15          Src    PCOffset9                0
    |-------------------------------------------|
    | 0 0 1 1 | S S S | P P P P P P P P P       |
    |-------------------------------------------|
    S S S = 3-bit Source Register
    P P P P P P P P P = PCOffset9

    Sign extend PCOffset9, add to PC, and read
    the value from the source register into
    that memory location
  */

  // Get the source register
  uint16_t source = (instruction >> 9) & 0x7;

  // Get PCOffset9
  uint16_t pcOffset9 = instruction & 0x1FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset9, 9);

  mem_write(registers[R_PC] + signExtendedPCOffset, registers[source]);
}

void storeIndirect(uint16_t instruction) {

  /* Instruction Format:
    15          Src    PCOffset9                0
    |-------------------------------------------|
    | 1 0 1 1 | S S S | P P P P P P P P P       |
    |-------------------------------------------|
    S S S = 3-bit Source Register
    P P P P P P P P P = PCOffset9

    Sign extend PCOffset9, add to PC to get an address. 
    Read the value from the source register and
    store in the computed address.
  */

  // Get the source register
  uint16_t source = (instruction >> 9) & 0x7;

  // Get PCOffset9
  uint16_t pcOffset9 = instruction & 0x1FF;
  uint16_t signExtendedPCOffset = sign_extend(pcOffset9, 9);

  uint16_t address = mem_read(registers[R_PC] + signExtendedPCOffset);

  mem_write(address, registers[source]);
}

void storeRegister(uint16_t instruction) {

  /* Instruction Format:
    15          Src    Base     Offset6         0
    |-------------------------------------------|
    | 0 1 1 1 | S S S | B B B | O O O O O O     |
    |-------------------------------------------|
    S S S = 3-bit Destination Register
    B B B = 3-bit Base Register
    O O O O O O = 6-bit offset

    Sign extend the offset, add this value to
    the value in the base register. Read the 
    value in the source register and store
    into memory at the computed address
  */
  // Get the source register
  uint16_t source = (instruction >> 9) & 0x7;

  // Get the base register
  uint16_t baseRegister = (instruction >> 6) & 0x7;

  // Get the offset
  uint16_t offset = instruction & 0x3F;
  uint16_t signExtendedOffset = sign_extend(offset, 6);

  uint16_t address = registers[baseRegister] + signExtendedOffset;

  mem_write(address, registers[source]);
}

/* TRAP functions */
void trapGetC() {
  registers[R_R0] = (uint16_t) getchar();
}

void trapHalt() {
  puts("HALT");
  fflush(stdout);
  running = 0;
}

void trapIn() {
  printf("Enter a character: ");
  registers[R_R0] = (uint16_t) getchar();
}

void trapOut() {
  putc((char)registers[R_R0], stdout);
  fflush(stdout);
}

void trapPuts() {
  uint16_t* character = memory + registers[R_R0];
  while (*character) {
    putc((char)*character, stdout);
    ++character;
  }
  fflush(stdout);
}

void trapPutSP() {
  /* One char per byte (two bytes per word)
  Convert to Big Endian format
   */
  uint16_t* character = memory + registers[R_R0];
  while (*character)
  {
      char char1 = (*character) & 0xFF;
      putc(char1, stdout);

      char char2 = (*character) >> 8;
      if (char2) putc(char2, stdout);
      ++character;
  }
  fflush(stdout);
}

void trap(uint16_t instruction) {
  uint16_t trapCode = instruction & 0xFF;

  switch(trapCode) {
    case TRAP_GETC:
      trapGetC();
      break;
    case TRAP_OUT:
      trapOut();
      break;
    case TRAP_PUTS:
      trapPuts();
      break;
    case TRAP_IN:
      trapIn();
      break;
    case TRAP_PUTSP:
      trapPutSP();
      break;
    case TRAP_HALT:
      trapHalt();
      break;
  }
}

/* MAIN */
int main(int argc, const char* argv[]) {

  if (argc < 2) {
    /* show usage string */
    printf("lc3 [image-file1] ...\n");
    exit(2);
  }

  for (int j = 1; j < argc; ++j) {
    if (!read_image(argv[j])) {
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

  running = 1;
  while (running) {
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
  }// end while

  restore_input_buffering();
}