#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include "compiler.h"

// Compile ld sp, imm16 - sets up SP pointer and sp_adjust
void compile_ld_sp_imm16(
    struct compile_ctx *ctx,
    struct code_block *block,
    uint16_t src_address,
    uint16_t *src_ptr
);

// Compile stack operations (push, pop, ld sp, ld hl,sp+n)
// Returns 1 if opcode was handled, 0 otherwise
int compile_stack_op(
    struct code_block *block,
    uint8_t op,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
);

#endif
