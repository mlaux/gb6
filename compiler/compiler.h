#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

// GB register -> 68k register mapping:
// A  -> D0 (low byte)
// BC -> D1
// DE -> D2
// HL -> A0
// SP -> A7 (68k convention)
// F  -> stored separately, computed as needed

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
