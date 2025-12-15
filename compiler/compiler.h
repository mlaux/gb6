#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

// D0 = A (GB accumulator)
// D1 = BC (split: 0x00BB00CC)
// D2 = DE (split: 0x00DD00EE)
// D3 = scratch
// D4 = dispatcher return value (next GB PC)
// D7 = flags (ZNHC0000)
// A0 = HL (contiguous: 0xHHLL)
// A1 = SP (base + SP, for direct stack access)
// A2 = scratch
// A3 = scratch (function pointers for JSR)
// A4 = runtime context pointer
// A5 = reserved (Mac "A5 world")
// A6 = reserved (Mac frame pointer)
// A7 = 68k stack pointer

#define REG_68K_D_A 1
#define REG_68K_D_BC 2
#define REG_68K_D_DE 3
#define REG_68K_D_NEXT_PC 4
#define REG_68K_D_SCRATCH_1 5
#define REG_68K_D_SCRATCH_2 6
#define REG_68K_D_FLAGS 7

#define REG_68K_A_HL 0
#define REG_68K_A_SP 1


struct code_block {
    uint8_t code[256];
    uint16_t m68k_offsets[256];
    size_t length;
    uint16_t gb_cycles; // for timing
    uint16_t src_address;  // GB address this block starts at
    uint16_t end_address;  // GB address after last instruction
};

// Initialize the compiler (call once at startup)
void compiler_init(void);

// Compile a basic block starting at the given GB address
// Returns NULL on failure
// The block ends when a control flow instruction is encountered
struct code_block *compile_block(uint16_t src_address, uint8_t *gb_code);

// Free a compiled block
void block_free(struct code_block *block);

// Emit helpers (exposed for testing)
void emit_byte(struct code_block *block, uint8_t byte);
void emit_word(struct code_block *block, uint16_t word);

#endif
