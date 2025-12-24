#ifndef ALU_H
#define ALU_H

#include <stdint.h>
#include "compiler.h"

// Compile 8-bit ALU operations
// Returns 1 if opcode was handled, 0 otherwise
int compile_alu_op(
    struct code_block *block,
    uint8_t op,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
);

#endif
