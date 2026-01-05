#include "alu.h"
#include "emitters.h"
#include "flags.h"
#include "interop.h"
#include "branches.h"

// helper for reading GB memory during compilation
#define READ_BYTE(off) (ctx->read(ctx->dmg, src_address + (off)))

// Try to fuse with a following conditional branch using live CCR flags.
// allow_carry: if true, allow C conditions (for cp/sub); if false, only Z (for and/or/tst)
// Returns 1 if fused, 0 if caller should save flags normally.
static int try_fuse_branch(
    struct code_block *block,
    struct compile_ctx *ctx,
    uint16_t *src_ptr,
    uint16_t src_address,
    int allow_carry
) {
    uint8_t next_op = READ_BYTE(*src_ptr);
    int cond = get_branch_condition(next_op);

    if (cond == COND_NONE)
        return 0;

    // For and/or/tst, only fuse Z conditions
    if (!allow_carry && (cond == COND_CS || cond == COND_CC))
        return 0;

    // Record m68k offset for branch instruction and consume opcode
    block->m68k_offsets[*src_ptr] = block->length;
    (*src_ptr)++;

    // Emit fused branch based on opcode type
    switch (next_op) {
    case 0x20: case 0x28: case 0x30: case 0x38:  // jr nz/z/nc/c
        compile_jr_cond_fused(block, ctx, src_ptr, src_address, cond);
        break;
    case 0xc2: case 0xca: case 0xd2: case 0xda:  // jp nz/z/nc/c
        compile_jp_cond_fused(block, ctx, src_ptr, src_address, cond);
        break;
    case 0xc4: case 0xcc: case 0xd4: case 0xdc:  // call nz/z/nc/c
        compile_call_cond_fused(block, ctx, src_ptr, src_address, cond);
        break;
    default:  // ret nz/z/nc/c
        compile_ret_cond_fused(block, cond);
        break;
    }
    return 1;
}

// ADC core: expects operand already in D1.b
// Does A = A + D1 + carry using 16-bit arithmetic
static void compile_adc_core(struct code_block *block)
{
    // Zero-extend operand: andi.w #0xff, D1
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_1, 0x00ff);

    // Zero-extend A into D2: moveq #0, D2; move.b D4, D2
    emit_moveq_dn(block, REG_68K_D_SCRATCH_0, 0);
    emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_SCRATCH_0);

    // Add operand: add.w D1, D2
    emit_add_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);

    // Test old carry and conditionally add 1
    emit_btst_imm_dn(block, 4, REG_68K_D_FLAGS);  // btst #4, D7
    emit_beq_b(block, 2);                          // beq +2 (skip addq)
    emit_addq_w_dn(block, REG_68K_D_SCRATCH_0, 1); // addq.w #1, D2

    // Store result: move.b D2, D4
    emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);

    // Set Z flag: tst.b D4; seq D1; andi.b #0x80, D1
    emit_tst_b_dn(block, REG_68K_D_A);
    emit_scc(block, 0x07, REG_68K_D_SCRATCH_1);  // seq
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80);

    // Set C flag from bit 8: btst #8, D2; sne D7; andi.b #0x10, D7
    emit_btst_imm_dn(block, 8, REG_68K_D_SCRATCH_0);
    emit_scc(block, 0x06, REG_68K_D_FLAGS);  // sne
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10);

    // Combine: or.b D1, D7 (Z | C, N=0, H=0)
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);
}

// SBC core: expects operand already in D1.b
// Does A = A - D1 - carry using 16-bit arithmetic
static void compile_sbc_core(struct code_block *block)
{
    // Zero-extend operand: andi.w #0xff, D1
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_1, 0x00ff);

    // Zero-extend A into D2: moveq #0, D2; move.b D4, D2
    emit_moveq_dn(block, REG_68K_D_SCRATCH_0, 0);
    emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_SCRATCH_0);

    // Subtract operand: sub.w D1, D2
    emit_sub_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);

    // Test old carry and conditionally subtract 1
    emit_btst_imm_dn(block, 4, REG_68K_D_FLAGS);  // btst #4, D7
    emit_beq_b(block, 2);                          // beq +2 (skip subq)
    emit_subq_w_dn(block, REG_68K_D_SCRATCH_0, 1); // subq.w #1, D2

    // Store result: move.b D2, D4
    emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);

    // Set Z flag: tst.b D4; seq D1; andi.b #0x80, D1
    emit_tst_b_dn(block, REG_68K_D_A);
    emit_scc(block, 0x07, REG_68K_D_SCRATCH_1);  // seq
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80);

    // Set C flag from bit 15 (borrow): btst #15, D2; sne D7; andi.b #0x10, D7
    emit_btst_imm_dn(block, 15, REG_68K_D_SCRATCH_0);
    emit_scc(block, 0x06, REG_68K_D_FLAGS);  // sne
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10);

    // Set N flag and combine: ori.b #0x40, D7; or.b D1, D7
    emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);
}

int compile_alu_op(
    struct code_block *block,
    uint8_t op,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    switch (op) {
    // inc/dec register ops (0xn4, 0xn5, 0xnc, 0xnd where n = 0-3)
    case 0x04: // inc b
        emit_swap(block, REG_68K_D_BC);
        emit_addq_b_dn(block, REG_68K_D_BC, 1);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        emit_swap(block, REG_68K_D_BC);
        return 1;

    case 0x05: // dec b
        emit_swap(block, REG_68K_D_BC);
        emit_subq_b_dn(block, REG_68K_D_BC, 1);
        compile_set_z_flag(block);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);
        emit_swap(block, REG_68K_D_BC);
        return 1;

    case 0x0c: // inc c
        emit_addq_b_dn(block, REG_68K_D_BC, 1);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        return 1;

    case 0x0d: // dec c
        emit_subq_b_dn(block, REG_68K_D_BC, 1);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 0)) {
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);
        }
        return 1;

    case 0x14: // inc d
        emit_swap(block, REG_68K_D_DE);
        emit_addq_b_dn(block, REG_68K_D_DE, 1);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        emit_swap(block, REG_68K_D_DE);
        return 1;

    case 0x15: // dec d
        emit_swap(block, REG_68K_D_DE);
        emit_subq_b_dn(block, REG_68K_D_DE, 1);
        compile_set_z_flag(block);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // N flag
        emit_swap(block, REG_68K_D_DE);
        return 1;

    case 0x1c: // inc e
        emit_addq_b_dn(block, REG_68K_D_DE, 1);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        return 1;

    case 0x1d: // dec e
        emit_subq_b_dn(block, REG_68K_D_DE, 1);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 0)) {
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);
        }
        return 1;

    case 0x24: // inc h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_addq_b_dn(block, REG_68K_D_SCRATCH_1, 1);
        emit_ror_w_8(block, REG_68K_D_SCRATCH_1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        return 1;

    case 0x25: // dec h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_subq_b_dn(block, REG_68K_D_SCRATCH_1, 1);
        emit_ror_w_8(block, REG_68K_D_SCRATCH_1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        compile_set_z_flag(block);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // N flag
        return 1;

    case 0x2c: // inc l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_addq_b_dn(block, REG_68K_D_SCRATCH_1, 1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        return 1;

    case 0x2d: // dec l (movea doesn't affect CCR, so we can fuse)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_subq_b_dn(block, REG_68K_D_SCRATCH_1, 1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 0)) {
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);
        }
        return 1;

    case 0x34: // inc (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);  // result in D0
        emit_addq_b_dn(block, 0, 1);
        compile_set_z_flag(block);  // capture Z immediately, preserves C
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);  // clear N
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_write_d0(block);
        return 1;

    case 0x35: // dec (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);  // result in D0
        emit_subq_b_dn(block, 0, 1);
        compile_set_z_flag(block);  // capture Z immediately, preserves C
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // set N
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_write_d0(block);
        return 1;

    case 0x3c: // inc a
        emit_addq_b_dn(block, REG_68K_D_A, 1);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
        return 1;

    case 0x3d: // dec a
        emit_subq_b_dn(block, REG_68K_D_A, 1);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 0)) {
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);
        }
        return 1;

    // misc ALU ops
    case 0x27: // daa - decimal adjust accumulator
        // TODO: implement DAA properly
        return 1;

    case 0x2f: // cpl - complement A
        emit_not_b_dn(block, REG_68K_D_A);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x60);  // set N and H
        return 1;

    case 0x37: // scf - set carry flag
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);  // keep Z, clear N, H
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x10);   // set C
        return 1;

    case 0x3f: // ccf - complement carry flag
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x90);  // keep Z and C, clear N, H
        emit_eor_b_imm_dn(block, 0x10, REG_68K_D_FLAGS);  // toggle C
        return 1;

    // 8-bit ALU register ops (0x80-0xbf)
    case 0x80: // add a, b
        emit_swap(block, REG_68K_D_BC);
        emit_add_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_zc_flags(block);  // capture before swap clobbers CCR
        emit_swap(block, REG_68K_D_BC);
        return 1;

    case 0x81: // add a, c
        emit_add_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_zc_flags(block);
        return 1;

    case 0x82: // add a, d
        emit_swap(block, REG_68K_D_DE);
        emit_add_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_zc_flags(block);  // capture before swap clobbers CCR
        emit_swap(block, REG_68K_D_DE);
        return 1;

    case 0x83: // add a, e
        emit_add_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_zc_flags(block);
        return 1;

    case 0x84: // add a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_add_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_zc_flags(block);
        return 1;

    case 0x85: // add a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_add_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_zc_flags(block);
        return 1;

    case 0x86: // add a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_add_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        compile_set_zc_flags(block);
        return 1;

    case 0x87: // add a, a
        emit_add_b_dn_dn(block, REG_68K_D_A, REG_68K_D_A);
        compile_set_zc_flags(block);
        return 1;

    // adc a, r (0x88-0x8f)
    case 0x88: // adc a, b
        emit_swap(block, REG_68K_D_BC);
        emit_move_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);
        emit_swap(block, REG_68K_D_BC);
        compile_adc_core(block);
        return 1;

    case 0x89: // adc a, c
        emit_move_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);
        compile_adc_core(block);
        return 1;

    case 0x8a: // adc a, d
        emit_swap(block, REG_68K_D_DE);
        emit_move_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_SCRATCH_1);
        emit_swap(block, REG_68K_D_DE);
        compile_adc_core(block);
        return 1;

    case 0x8b: // adc a, e
        emit_move_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_SCRATCH_1);
        compile_adc_core(block);
        return 1;

    case 0x8c: // adc a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        compile_adc_core(block);
        return 1;

    case 0x8d: // adc a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_adc_core(block);
        return 1;

    case 0x8e: // adc a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_1);
        compile_adc_core(block);
        return 1;

    case 0x8f: // adc a, a
        emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_SCRATCH_1);
        compile_adc_core(block);
        return 1;

    case 0x90: // sub a, b
        emit_swap(block, REG_68K_D_BC);
        emit_sub_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_znc_flags(block);  // capture before swap clobbers CCR
        emit_swap(block, REG_68K_D_BC);
        return 1;

    case 0x91: // sub a, c
        emit_sub_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_znc_flags(block);
        return 1;

    case 0x92: // sub a, d
        emit_swap(block, REG_68K_D_DE);
        emit_sub_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_znc_flags(block);  // capture before swap clobbers CCR
        emit_swap(block, REG_68K_D_DE);
        return 1;

    case 0x93: // sub a, e
        emit_sub_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_znc_flags(block);
        return 1;

    case 0x94: // sub a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_sub_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_znc_flags(block);
        return 1;

    case 0x95: // sub a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_sub_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_znc_flags(block);
        return 1;

    case 0x96: // sub a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_sub_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        compile_set_znc_flags(block);
        return 1;

    case 0x97: // sub a, a - always results in 0
        emit_moveq_dn(block, REG_68K_D_A, 0);
        emit_move_b_dn(block, REG_68K_D_FLAGS, 0xc0);  // Z=1, N=1, H=0, C=0
        return 1;

    // sbc a, r (0x98-0x9f)
    case 0x98: // sbc a, b
        emit_swap(block, REG_68K_D_BC);
        emit_move_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);
        emit_swap(block, REG_68K_D_BC);
        compile_sbc_core(block);
        return 1;

    case 0x99: // sbc a, c
        emit_move_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);
        compile_sbc_core(block);
        return 1;

    case 0x9a: // sbc a, d
        emit_swap(block, REG_68K_D_DE);
        emit_move_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_SCRATCH_1);
        emit_swap(block, REG_68K_D_DE);
        compile_sbc_core(block);
        return 1;

    case 0x9b: // sbc a, e
        emit_move_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_SCRATCH_1);
        compile_sbc_core(block);
        return 1;

    case 0x9c: // sbc a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        compile_sbc_core(block);
        return 1;

    case 0x9d: // sbc a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_sbc_core(block);
        return 1;

    case 0x9e: // sbc a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_1);
        compile_sbc_core(block);
        return 1;

    case 0x9f: // sbc a, a
        emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_SCRATCH_1);
        compile_sbc_core(block);
        return 1;

    case 0xa0: // and a, b
        emit_swap(block, REG_68K_D_BC);
        emit_and_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_swap(block, REG_68K_D_BC);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);  // keep Z, clear N, C
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);   // set H
        return 1;

    case 0xa1: // and a, c
        emit_and_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xa2: // and a, d
        emit_swap(block, REG_68K_D_DE);
        emit_and_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_swap(block, REG_68K_D_DE);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xa3: // and a, e
        emit_and_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xa4: // and a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_and_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xa5: // and a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_and_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xa6: // and a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_and_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xa7: // and a, a - set flags based on A
        emit_tst_b_dn(block, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 0)) {
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        }
        return 1;

    case 0xa8: // xor a, b
        emit_swap(block, REG_68K_D_BC);
        emit_eor_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_swap(block, REG_68K_D_BC);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xa9: // xor a, c
        emit_eor_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xaa: // xor a, d
        emit_swap(block, REG_68K_D_DE);
        emit_eor_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_swap(block, REG_68K_D_DE);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xab: // xor a, e
        emit_eor_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xac: // xor a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_eor_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xad: // xor a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_eor_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xae: // xor a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_eor_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xaf: // xor a, a - always results in 0, Z=1
        emit_moveq_dn(block, REG_68K_D_A, 0);
        emit_move_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb0: // or a, b
        emit_swap(block, REG_68K_D_BC);
        emit_or_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_swap(block, REG_68K_D_BC);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb1: // or a, c
        emit_or_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb2: // or a, d
        emit_swap(block, REG_68K_D_DE);
        emit_or_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_swap(block, REG_68K_D_DE);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb3: // or a, e
        emit_or_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb4: // or a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb5: // or a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb6: // or a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xb7: // or a, a - set flags based on A
        emit_tst_b_dn(block, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 0)) {
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        }
        return 1;

    case 0xb8: // cp a, b
        // cannot fuse branch because it needs to swap back BC :/
        emit_swap(block, REG_68K_D_BC);
        emit_cmp_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        compile_set_znc_flags(block);
        emit_swap(block, REG_68K_D_BC);
        return 1;

    case 0xb9: // cp a, c
        emit_cmp_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 1))
            compile_set_znc_flags(block);
        return 1;

    case 0xba: // cp a, d 
        // can't fuse
        emit_swap(block, REG_68K_D_DE);
        emit_cmp_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        compile_set_znc_flags(block);
        emit_swap(block, REG_68K_D_DE);
        return 1;

    case 0xbb: // cp a, e
        emit_cmp_b_dn_dn(block, REG_68K_D_DE, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 1))
            compile_set_znc_flags(block);
        return 1;

    case 0xbc: // cp a, h
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        emit_cmp_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 1))
            compile_set_znc_flags(block);
        return 1;

    case 0xbd: // cp a, l
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_cmp_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 1))
            compile_set_znc_flags(block);
        return 1;

    case 0xbe: // cp a, (hl)
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);
        emit_cmp_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 1))
            compile_set_znc_flags(block);
        return 1;

    case 0xbf: // cp a, a - always Z=1, N=1, H=0, C=0
        emit_move_b_dn(block, REG_68K_D_FLAGS, 0xc0);
        return 1;

    // immediate ALU ops (0xc6, 0xce, 0xd6, 0xde, 0xe6, 0xee, 0xf6, 0xfe)
    case 0xc6: // add a, #imm
        emit_addi_b_dn(block, REG_68K_D_A, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        compile_set_zc_flags(block);
        return 1;

    case 0xce: // adc a, #imm
        emit_move_b_dn(block, REG_68K_D_SCRATCH_1, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        compile_adc_core(block);
        return 1;

    case 0xd6: // sub a, #imm
        emit_subi_b_dn(block, REG_68K_D_A, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        compile_set_znc_flags(block);
        return 1;

    case 0xde: // sbc a, #imm
        emit_move_b_dn(block, REG_68K_D_SCRATCH_1, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        compile_sbc_core(block);
        return 1;

    case 0xe6: // and a, #nn
        emit_andi_b_dn(block, REG_68K_D_A, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);
        emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);
        return 1;

    case 0xee: // xor a, u8
        emit_eor_b_imm_dn(block, READ_BYTE(*src_ptr), REG_68K_D_A);
        (*src_ptr)++;
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xf6: // or a, #imm
        emit_ori_b_dn(block, REG_68K_D_A, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        compile_set_z_flag(block);
        emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
        return 1;

    case 0xfe: // cp a, imm8
        emit_cmp_b_imm_dn(block, REG_68K_D_A, READ_BYTE(*src_ptr));
        (*src_ptr)++;
        if (!try_fuse_branch(block, ctx, src_ptr, src_address, 1))
            compile_set_znc_flags(block);
        return 1;

    default:
        return 0;
    }
}
