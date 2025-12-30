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
        // Backward jump within block
        target_gb_pc = src_address + target_gb_offset;
        target_m68k = block->m68k_offsets[target_gb_offset];
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);

        // Tiny loops (disp >= -3, e.g. "dec a; jr nz") are pure computation
        // (no room for memory access + flag-setting instruction).
        // Skip interrupt check to avoid overhead killing performance.
        if (disp >= -3) {
            emit_bra_w(block, m68k_disp);
            return 0;
        }

        // Larger loop - check interrupt flag, exit to dispatcher if set
        // tst.b JIT_CTX_INTCHECK(a4)
        emit_tst_b_disp_an(block, JIT_CTX_INTCHECK, REG_68K_A_CTX);
        // beq.w over exit sequence to bra.w (skip moveq(2) + move.w(4) + dispatch_jump(6) = 12, plus 2 = 14)
        emit_beq_w(block, 14);
        // Exit to dispatcher with target PC
        emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
        emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
        emit_dispatch_jump(block);

        // Native branch (interrupt flag was clear)
        // Must recompute displacement since block->length changed
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);
        emit_bra_w(block, m68k_disp);
        return 0;
    }

    // Forward jump or outside block - go through dispatcher
    target_gb_pc = src_address + target_gb_offset;
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
    emit_dispatch_jump(block);
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
        // Backward jump - check condition, then maybe interrupt flag
        target_gb_pc = src_address + target_gb_offset;
        target_m68k = block->m68k_offsets[target_gb_offset];
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);

        // Tiny loops (disp >= -3): skip interrupt check, just branch
        if (disp >= -3) {
            // bxx.w displacement needs adjustment for where we emit it
            int16_t cond_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);
            if (branch_if_set) {
                emit_bne_w(block, cond_disp);
            } else {
                emit_beq_w(block, cond_disp);
            }
            return;
        }

        // Larger loop - check condition, then interrupt flag
        // Structure:
        //   btst #flag_bit, d7           ; already emitted above
        //   bne/beq .check_interrupt     ; if condition met, check interrupt
        //   bra.w .fall_through          ; condition not met, skip all
        // .check_interrupt:
        //   tst.b JIT_CTX_INTCHECK(a4)
        //   beq.w loop_target            ; no interrupt, do native branch
        //   moveq #0, d0                 ; interrupt pending, exit
        //   move.w #target, d0
        //   dispatch_jump
        // .fall_through:

        // Sizes: bne/beq(4) + bra.w(4) + tst.b(4) + beq.w(4) + moveq(2) + move.w(4) + dispatch_jump(6) = 28
        // .check_interrupt is at +8 from first branch
        // .fall_through is at +28 from first branch

        if (branch_if_set) {
            // Branch if flag is set: btst gives Z=0 when bit=1, so use bne
            emit_bne_w(block, 6);  // skip to .check_interrupt (+8, but PC is at +2)
        } else {
            // Branch if flag is clear: btst gives Z=1 when bit=0, so use beq
            emit_beq_w(block, 6);
        }

        // bra.w to .fall_through (tst.b(4) + beq.w(4) + moveq(2) + move.w(4) + dispatch_jump(6) = 20, plus 2 for PC = 22)
        emit_bra_w(block, 22);

        // .check_interrupt:
        emit_tst_b_disp_an(block, JIT_CTX_INTCHECK, REG_68K_A_CTX);

        // beq.w to native loop target (no interrupt pending)
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);
        emit_beq_w(block, m68k_disp);

        // Exit to dispatcher (interrupt pending)
        emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
        emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
        emit_dispatch_jump(block);

        // .fall_through: block continues
        return;
    }

    // Forward/external jump - conditionally exit to dispatcher
    // If condition NOT met, skip the exit sequence
    target_gb_pc = src_address + target_gb_offset;

    if (branch_if_set) {
        // Skip exit if flag is clear (btst Z=1 when bit=0)
        emit_beq_w(block, 14);  // skip: moveq(2) + move.w(4) + dispatch_jump(6) = 12, plus 2 = 14
    } else {
        // Skip exit if flag is set (btst Z=0 when bit=1)
        emit_bne_w(block, 14);
    }

    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
    emit_dispatch_jump(block);
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
        emit_beq_w(block, 14);  // skip: moveq(2) + move.w(4) + dispatch_jump(6) = 12, plus 2 = 14
    } else {
        // Skip exit if flag is set (btst Z=0 when bit=1)
        emit_bne_w(block, 14);
    }

    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
    emit_dispatch_jump(block);
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
    emit_dispatch_jump(block);
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
    emit_dispatch_jump(block);
}

// Compile conditional return (ret nz, ret z, ret nc, ret c)
// flag_bit: which bit in D7 to test (7=Z, 4=C)
// branch_if_set: if true, return when flag is set; if false, return when clear
void compile_ret_cond(struct code_block *block, uint8_t flag_bit, int branch_if_set)
{
    // Test the flag bit in D7
    emit_btst_imm_dn(block, flag_bit, 7);

    // If condition NOT met, skip the return sequence
    // Return sequence is 18 bytes: moveq(2) + move.b d(An),Dn(4) + rol.w(2) +
    //                              move.b (An),Dn(2) + addq.w(2) + dispatch_jump(6)
    // bxx.w displacement is relative to PC after opcode word, so add 2
    if (branch_if_set) {
        // Skip return if flag is clear (btst Z=1 when bit=0)
        emit_beq_w(block, 20);
    } else {
        // Skip return if flag is set (btst Z=0 when bit=1)
        emit_bne_w(block, 20);
    }

    // Pop return address and return (same as compile_ret)
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_NEXT_PC);
    emit_rol_w_8(block, REG_68K_D_NEXT_PC);
    emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_NEXT_PC);
    emit_addq_w_an(block, REG_68K_A_SP, 2);
    emit_dispatch_jump(block);
}

void compile_rst_n(struct code_block *block, uint8_t target, uint16_t ret_addr)
{
    // push return address
    emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
    emit_move_w_dn(block, REG_68K_D_SCRATCH_1, ret_addr);
    emit_subq_w_an(block, REG_68K_A_SP, 2);
    emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
    emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
    emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);
    // jump to target (0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38)
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, target);
    emit_dispatch_jump(block);
}