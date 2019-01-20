#ifndef _INSTRUCTION_SET
#define _INSTRUCTION_SET

#include <stdint.h>

#include "../core/opcodes.h"

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