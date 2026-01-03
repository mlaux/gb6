#ifndef _BRANCHES_H
#define _BRANCHES_H

// Native 68k condition codes for fused compare+branch
// These match 68k Bcc encoding: beq=7, bne=6, bcs=5, bcc=4
#define COND_CC  4   // carry clear (nc)
#define COND_CS  5   // carry set (c)
#define COND_NE  6   // not equal/not zero (nz)
#define COND_EQ  7   // equal/zero (z)
#define COND_NONE -1 // not a conditional branch

// Returns 68k condition code if opcode is a conditional branch, COND_NONE otherwise
int get_branch_condition(uint8_t opcode);

// Fused versions that use live CCR flags instead of btst on D7
// Returns 1 if this ended the block, 0 otherwise
int compile_jr_cond_fused(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    int cond
);

void compile_jp_cond_fused(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    int cond
);

void compile_ret_cond_fused(struct code_block *block, int cond);

int compile_jr(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address
);

void compile_jr_cond(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    uint8_t flag_bit,
    int branch_if_set
);

void compile_jp_cond(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    uint8_t flag_bit,
    int branch_if_set
);

void compile_call_imm16(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address
);

void compile_call_cond(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    uint8_t flag_bit,
    int branch_if_set
);

void compile_call_cond_fused(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    int cond
);

void compile_ret(struct code_block *block);
void compile_ret_cond(struct code_block *block, uint8_t flag_bit, int branch_if_set);
void compile_rst_n(struct code_block *block, uint8_t target, uint16_t ret_addr);

#endif