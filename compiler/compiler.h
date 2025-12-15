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

#define REG_68K_D_A 1
#define REG_68K_D_BC 2
#define REG_68K_D_DE 3
#define REG_68K_D_NEXT_PC 4
#define REG_68K_D_SCRATCH_1 5
#define REG_68K_D_SCRATCH_2 6
#define REG_68K_D_FLAGS 7

#define REG_68K_A_HL 0
#define REG_68K_A_SP 1
#define REG_68K_A_SCRATCH_1 2
#define REG_68K_A_SCRATCH_2 3
#define REG_68K_A_CTX 4

// Runtime context offsets
#define JIT_CTX_DMG   0
#define JIT_CTX_READ  4
#define JIT_CTX_WRITE 8

// Register save masks for movem around C function calls
// Retro68 (gcc) may clobber D0-D2, A0-A1, so we save our working regs
// For write: save D1(A), D2(BC), D3(DE), D7(flags), A0(HL), A1(SP)
// Predec mask: bit 15=D0..8=D7, 7=A0..0=A7
//   D1=bit14, D2=bit13, D3=bit12, D7=bit8, A0=bit7, A1=bit6
#define MOVEM_SAVE_WRITE_PREDEC  0x71c0
//   D1=bit1, D2=bit2, D3=bit3, D7=bit7, A0=bit8, A1=bit9
#define MOVEM_SAVE_WRITE_POSTINC 0x038e

// For read: same but don't save D1 since we'll overwrite it with result
//   D2=bit13, D3=bit12, D7=bit8, A0=bit7, A1=bit6
#define MOVEM_SAVE_READ_PREDEC   0x31c0
//   D2=bit2, D3=bit3, D7=bit7, A0=bit8, A1=bit9
#define MOVEM_SAVE_READ_POSTINC  0x038c


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
