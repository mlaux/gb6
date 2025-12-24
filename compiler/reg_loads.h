#ifndef REG_LOADS_H
#define REG_LOADS_H

#include <stdint.h>
#include "compiler.h"

// Compile a register-to-register load (opcodes 0x40-0x7f)
// Returns 1 if successfully compiled, 0 if unknown opcode (e.g., 0x76 HALT)
int compile_reg_load(struct code_block *block, uint8_t op);

#endif
