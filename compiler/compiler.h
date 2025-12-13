#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

// GB register -> 68k register mapping:
// D0 = A (8-bit)
// D1 = BC (split: 0x00BB00CC)
// D2 = DE (split: 0x00DD00EE)
// D3 = scratch
// D7 = flags (ZNHC0000)
// A0 = HL (contiguous: 0xHHLL)
// A7 = SP, might need to move to preserve mac stack

struct basic_block {
    uint8_t code[256];
    size_t length;
    uint16_t src_address;  // GB address this block starts at
    uint16_t end_address;  // GB address after last instruction
};

// Initialize the compiler (call once at startup)
void compiler_init(void);

// Compile a basic block starting at the given GB address
// Returns NULL on failure
// The block ends when a control flow instruction is encountered
struct basic_block *compile_block(uint16_t src_address, uint8_t *gb_code);

// Free a compiled block
void block_free(struct basic_block *block);

// Emit helpers (exposed for testing)
void emit_byte(struct basic_block *block, uint8_t byte);
void emit_word(struct basic_block *block, uint16_t word);

#endif
