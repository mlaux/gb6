#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

// D0 = dispatcher return value (next GB PC)
// D1 = scratch
// D2 = scratch
// D3 = scratch
// D4 = A (GB accumulator)
// D5 = BC (split: 0x00BB00CC)
// D6 = DE (split: 0x00DD00EE)
// D7 = flags (ZNHC0000)

// A0 = scratch
// A1 = scratch
// A2 = HL (contiguous: 0xHHLL)
// A3 = SP (base + SP, for direct stack access)
// A4 = runtime context pointer
// A5 = reserved (Mac "A5 world")
// A6 = reserved (Mac frame pointer)
// A7 = 68k stack pointer

#define REG_68K_D_NEXT_PC 0
#define REG_68K_D_SCRATCH_1 1
#define REG_68K_D_SCRATCH_2 2
#define REG_68K_D_SCRATCH_3 3
#define REG_68K_D_A 4
#define REG_68K_D_BC 5
#define REG_68K_D_DE 6
#define REG_68K_D_FLAGS 7

#define REG_68K_A_SCRATCH_1 0
#define REG_68K_A_SCRATCH_2 1
#define REG_68K_A_HL 2
#define REG_68K_A_SP 3
#define REG_68K_A_CTX 4

// Runtime context offsets
#define JIT_CTX_DMG   0
#define JIT_CTX_READ  4
#define JIT_CTX_WRITE 8


struct code_block {
    uint8_t code[256];
    uint16_t m68k_offsets[256];
    size_t length;
    uint16_t gb_cycles; // for timing
    uint16_t src_address;
    uint16_t end_address; // address after last instruction

    // set when compilation hits unknown opcode
    uint8_t error;
    uint8_t failed_opcode;
    uint16_t failed_address;
};

// base pointers for address calculation
struct compile_ctx {
    void *wram_base;
    void *hram_base;
};

void compiler_init(void);

struct code_block *compile_block(
    uint16_t src_address,
    uint8_t *gb_code,
    struct compile_ctx *ctx
);

// Free a compiled block
void block_free(struct code_block *block);

// Emit helpers (exposed for testing)
void emit_byte(struct code_block *block, uint8_t byte);
void emit_word(struct code_block *block, uint16_t word);

#endif
