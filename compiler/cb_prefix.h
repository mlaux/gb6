#ifndef CB_PREFIX_H
#define CB_PREFIX_H

#include <stdint.h>
#include "compiler.h"

// Compile a CB-prefixed instruction (BIT, RES, SET, and rotates/shifts)
// Returns 1 if successfully compiled, 0 if unknown opcode
int compile_cb_insn(struct code_block *block, uint8_t cb_op);

#endif
