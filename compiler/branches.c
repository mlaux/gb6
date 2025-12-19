#include <stdint.h>
#include "compiler.h"
#include "branches.h"
#include "emitters.h"

// helper for reading GB memory during compilation
#define READ_BYTE(off) (ctx->read(ctx->dmg, src_address + (off)))

// returns 1 if jr ended the block, 0 if it's a backward jump within block
int compile_jr(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address
) {
    int8_t disp;
    int16_t target_gb_offset;
    uint16_t target_m68k, target_gb_pc;
    int16_t m68k_disp;

    disp = (int8_t) READ_BYTE(*src_ptr);
    (*src_ptr)++;

    // jr displacement is relative to PC after the jr instruction
    // *src_ptr now points to the byte after jr, so target = *src_ptr + disp
    target_gb_offset = (int16_t) *src_ptr + disp;

    // Check if this is a backward jump to a location we've already compiled
    if (target_gb_offset >= 0 && target_gb_offset < (int16_t) (*src_ptr - 2)) {
        // Backward jump within block - emit bra.w
        target_m68k = block->m68k_offsets[target_gb_offset];
        // bra.w displacement is relative to PC after the opcode word (before extension)
        // current position = block->length, PC after opcode = block->length + 2
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);
        emit_bra_w(block, m68k_disp);
        return 0;
    }

    // Forward jump or outside block - go through dispatcher
    target_gb_pc = src_address + target_gb_offset;
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
    emit_rts(block);
    return 1;
}

// Compile conditional relative jump (jr nz, jr z, jr nc, jr c)
// flag_bit: which bit in D7 to test (7=Z, 4=C)
// branch_if_set: if true, branch when flag is set; if false, branch when clear
void compile_jr_cond(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    uint8_t flag_bit,
    int branch_if_set
) {
    int8_t disp;
    int16_t target_gb_offset;
    uint16_t target_m68k, target_gb_pc;
    int16_t m68k_disp;

    disp = (int8_t) READ_BYTE(*src_ptr);
    (*src_ptr)++;

    target_gb_offset = (int16_t) *src_ptr + disp;

    // Test the flag bit in D7
    // btst sets 68k Z=1 if tested bit is 0, Z=0 if tested bit is 1
    emit_btst_imm_dn(block, flag_bit, 7);

    // Check if this is a backward jump within block
    if (target_gb_offset >= 0 && target_gb_offset < (int16_t) (*src_ptr - 2)) {
        // Backward jump - emit conditional branch
        target_m68k = block->m68k_offsets[target_gb_offset];
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);

        if (branch_if_set) {
            // Branch if flag is set: btst gives Z=0 when bit=1, so use bne
            emit_bne_w(block, m68k_disp);
        } else {
            // Branch if flag is clear: btst gives Z=1 when bit=0, so use beq
            emit_beq_w(block, m68k_disp);
        }
        return; // block continues (fall-through path)
    }

    // Forward/external jump - conditionally exit to dispatcher
    // If condition NOT met, skip the exit sequence
    target_gb_pc = src_address + target_gb_offset;

    if (branch_if_set) {
        // Skip exit if flag is clear (btst Z=1 when bit=0)
        emit_beq_w(block, 10);  // skip: moveq(2) + move.w(4) + rts(2) + ext(2) = 10
    } else {
        // Skip exit if flag is set (btst Z=0 when bit=1)
        emit_bne_w(block, 10);
    }

    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
    emit_rts(block);
}

// Compile conditional absolute jump (jp nz, jp z, jp nc, jp c)
// flag_bit: which bit in D7 to test (7=Z, 4=C)
// branch_if_set: if true, branch when flag is set; if false, branch when clear
void compile_jp_cond(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    uint8_t flag_bit,
    int branch_if_set
) {
    uint16_t target = READ_BYTE(*src_ptr) | (READ_BYTE(*src_ptr + 1) << 8);
    *src_ptr += 2;

    // Test the flag bit in D7
    emit_btst_imm_dn(block, flag_bit, 7);

    // If condition NOT met, skip the exit sequence
    if (branch_if_set) {
        // Skip exit if flag is clear (btst Z=1 when bit=0)
        emit_beq_w(block, 10);  // skip: moveq(2) + move.w(4) + rts(2) + ext(2) = 10
    } else {
        // Skip exit if flag is set (btst Z=0 when bit=1)
        emit_bne_w(block, 10);
    }

    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
    emit_rts(block);
}

void compile_call_imm16(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address
) {
    uint16_t target = READ_BYTE(*src_ptr) | (READ_BYTE(*src_ptr + 1) << 8);
    uint16_t ret_addr = src_address + *src_ptr + 2;  // address after call
    *src_ptr += 2;

    // push return address (A3 = base + SP)
    // use byte operations to handle odd SP addresses
    emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
    emit_move_w_dn(block, REG_68K_D_SCRATCH_1, ret_addr);

    // SP -= 2
    emit_subq_w_an(block, REG_68K_A_SP, 2);

    // [SP] = low byte
    emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
    // swap bytes
    emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
    // [SP+1] = high byte
    emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);

    // jump to target
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
    emit_rts(block);
}

void compile_ret(struct code_block *block)
{
    // pop return address from stack (A1 = base + SP)
    // need to use byte operations here bc GB stack pointer can be odd
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    // D0 = [SP+1] (high byte)
    emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_NEXT_PC);
     // shift to high position
    emit_rol_w_8(block, REG_68K_D_NEXT_PC);
    // D0.b = [SP] (low byte)
    emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_NEXT_PC);
    // SP += 2
    emit_addq_w_an(block, REG_68K_A_SP, 2);
    emit_rts(block);
}