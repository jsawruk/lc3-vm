// Includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#include "../core/core.h"
#include "../core/opcodes.h"

// Cannot include implementation files from core library
// when using templates
// See: https://stackoverflow.com/questions/51972934/macos-and-cmake-undefined-symbols-for-architecture-x86-64
// And: https://stackoverflow.com/questions/1724036/splitting-templated-c-classes-into-hpp-cpp-files-is-it-possible

// BIT UTILITIES
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

// READ IMAGES
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

// CORE FUNCTIONS
uint16_t check_key() {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
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

// INPUT BUFFERING
/* Struct for input buffering */
struct termios original_tio;

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

// C++ fetch-execute using templates
template <unsigned op>
void ins(uint16_t instruction) {
  
  uint16_t register0;
  uint16_t register1;
  uint16_t register2;

  uint16_t immediateValue_5;
  uint16_t immediateFlag;

  uint16_t pcPlusOffset;
  uint16_t basePlusOffset;

  uint16_t opbit = (1 << op);

  // Read in the register values
  if (0x4EEE & opbit) {
    register0 = (instruction >> 9) & 0x7;
  }

  if (0x12E3 & opbit) {
    register1 = (instruction >> 6) & 0x7;
  }

  if (0x0022 & opbit) {
    register2 = instruction & 0x7;
    immediateFlag = (instruction >> 5) & 0x1;
    immediateValue_5 = sign_extend((instruction) & 0x1F, 5);
  }

  if (0x00C0 & opbit) {
    // Base + offset
    basePlusOffset = registers[register1] + sign_extend(instruction & 0x3F, 6);
  }

  if (0x4C0D & opbit) {
    // Indirect address
    pcPlusOffset = registers[R_PC] + sign_extend(instruction & 0x1FF, 9);
  }

  // Instructions
  if (0x0001 & opbit) {
    // BR
    uint16_t condition = (instruction >> 9) & 0x7;
    if (condition & registers[R_COND]) {
      registers[R_PC] = pcPlusOffset;
    }
  }

  if (0x0002 & opbit) {
    // ADD
    if (immediateFlag) {
      registers[register0] = registers[register1] + immediateValue_5;
    }
    else {
      registers[register0] = registers[register1] + registers[register2];
    }
  }

  if (0x0020 & opbit) {
    // AND
    if (immediateFlag) {
      registers[register0] = registers[register1] & immediateValue_5;
    }
    else {
      registers[register0] = registers[register1] & registers[register2];
    }
  }

  if (0x0200 & opbit) {
    // NOT
    registers[register0] = ~registers[register1];
  }

  if (0x1000 & opbit) {
    // JMP
    registers[R_PC] = registers[register1];
  }

  if (0x0010 & opbit) {
    // JSR
    uint16_t longFlag = (instruction >> 11) & 1;
    pcPlusOffset = registers[R_PC] + sign_extend(instruction & 0x7FF, 11);
    registers[R_R7] = registers[R_PC];

    if (longFlag) {
      registers[R_PC] = pcPlusOffset;
    }
    else {
      registers[R_PC] = registers[register1];
    }
  }

  if (0x0004 & opbit) {
    // LD
    registers[register0] = mem_read(pcPlusOffset); 
  }

  if (0x0400 & opbit) {
    // LDI
    registers[register0] = mem_read(mem_read(pcPlusOffset));
  }

  if (0x0040 & opbit) {
    // LDR
    registers[register0] = mem_read(basePlusOffset);
  }
  
  if (0x4000 & opbit) {
    // LEA
    registers[register0] = pcPlusOffset;
  }

  if (0x0008 & opbit) {
    // ST
    mem_write(pcPlusOffset, registers[register0]);
  }

  if (0x0800 & opbit) {
    // STI
    mem_write(mem_read(pcPlusOffset), registers[register0]);
  }


  if (0x0080 & opbit) {
    // STR
    mem_write(basePlusOffset, registers[register0]);
  }

  if (0x8000 & opbit) {
    // TRAP
    switch (instruction & 0xFF) {
      case TRAP_GETC:
        // read a single ASCII char
        registers[R_R0] = (uint16_t) getchar();
        break;
      
      case TRAP_OUT:
        putc((char) registers[R_R0], stdout);
        fflush(stdout);
        break;
             
      case TRAP_PUTS:
        {
          // one char per word
          uint16_t* c = memory + registers[R_R0];
          while (*c) {
            putc((char) *c, stdout);
            ++c;
          }
          fflush(stdout);
        }
        break;

      case TRAP_IN:
        printf("Enter a character: ");
        registers[R_R0] = (uint16_t )getchar();
        break;
             
      case TRAP_PUTSP:
        /* one char per byte (two bytes per word)
          here we need to swap back to
          big endian format */
        {
          uint16_t* c = memory + registers[R_R0];
          while (*c) {
            char char1 = (*c) & 0xFF;
            putc(char1, stdout);
            char char2 = (*c) >> 8;
            if (char2) { 
              putc(char2, stdout);
            }
            ++c;
          } // end while *c
          fflush(stdout);
        }
        break;
      
      case TRAP_HALT:
        puts("HALT");
        fflush(stdout);
        break;
      } // end switch
    } // end if TRAP

  //if (0x0100 & opbit) { } // RTI
  if (0x4666 & opbit) { 
    update_flags(register0); 
  }
}

// OP Table
static void (*op_table[16])(uint16_t) = {
    ins<0>, ins<1>, ins<2>, ins<3>,
    ins<4>, ins<5>, ins<6>, ins<7>,
    NULL, ins<9>, ins<10>, ins<11>,
    ins<12>, NULL, ins<14>, ins<15>
};

// MAIN
int main(int argc, const char* argv[]) {

  if (argc < 2) {
    // show usage string
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

  // C++ fetch-execute
  while(1) {
    uint16_t instruction = mem_read(registers[R_PC]++);
    uint16_t opcode = instruction >> 12;
    op_table[opcode](instruction);
  }

  restore_input_buffering();
}