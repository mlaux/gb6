#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

// D0 = scratch/C interop return value
// D1 = scratch
// D2 = accumulated cycle count
// D3 = dispatcher return value (next GB PC)
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

#define REG_68K_D_SCRATCH_0 0
#define REG_68K_D_SCRATCH_1 1
#define REG_68K_D_SCRATCH_2 2
#define REG_68K_D_NEXT_PC 3
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
#define JIT_CTX_DMG         0
#define JIT_CTX_READ        4
#define JIT_CTX_WRITE       8
#define JIT_CTX_EI_DI       12
#define JIT_CTX_INTCHECK    16  // 1 byte
#define JIT_CTX_ROM_BANK    17  // 1 byte (current ROM bank for MBC)
// 2 bytes padding to align to 4 bytes
#define JIT_CTX_BANK0_CACHE   20  // struct code_block **bank0_cache
#define JIT_CTX_BANKED_CACHE  24  // struct code_block ***banked_cache
#define JIT_CTX_UPPER_CACHE   28  // struct code_block **upper_cache
#define JIT_CTX_DISPATCH      32  // void *dispatcher_return
#define JIT_CTX_UNUSED_1      36  // was sp_adjust
#define JIT_CTX_UNUSED_2      40  // was gb_sp
// 2 bytes padding at 42
#define JIT_CTX_CYCLES        44  // u32: accumulated GB cycles
#define JIT_CTX_PATCH_HELPER  48  // void *patch_helper routine
#define JIT_CTX_READ_CYCLES   52  // u32: in-flight cycles at dmg_read call
#define JIT_CTX_HRAM_BASE     56  // void *hram_base (dmg->zero_page)

struct code_block {
    uint8_t code[1024];
    uint16_t m68k_offsets[256];
    size_t length;
    uint16_t src_address;
    uint16_t end_address; // address after last instruction

    // estimated GB cycles for this block (assumes no branches taken)
    uint16_t gb_cycles;

    // set when compilation hits unknown opcode
    uint8_t error;
    uint16_t failed_opcode;
    uint16_t failed_address;
};

// read function signature: u8 (*read)(void *dmg, u16 address)
typedef uint8_t (*dmg_read_fn)(void *dmg, uint16_t address);

// compile-time context
struct compile_ctx {
    void *dmg;              // dmg pointer for memory reads
    dmg_read_fn read;       // read function
    int single_instruction; // if set, compile only one instruction then dispatch
};

void compiler_init(void);

struct code_block *compile_block(uint16_t src_address, struct compile_ctx *ctx);

// Free a compiled block
void block_free(struct code_block *block);

// Emit helpers (exposed for testing)
void emit_byte(struct code_block *block, uint8_t byte);
void emit_word(struct code_block *block, uint16_t word);

void compile_bc_to_addr(struct code_block *block);
void compile_de_to_addr(struct code_block *block);

extern int cycles_per_exit;

#endif
