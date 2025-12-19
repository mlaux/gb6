#include <stdint.h>

#include "cb_prefix.h"
#include "compiler.h"
#include "emitters.h"
#include "flags.h"
#include "interop.h"

// GB register encoding in CB-prefix instructions:
// 0=B, 1=C, 2=D, 3=E, 4=H, 5=L, 6=(HL), 7=A
//
// 68k register mapping:
// D4 = A
// D5 = BC in split format: 0x00BB00CC (B in bits 16-23, C in bits 0-7)
// D6 = DE in split format: 0x00DD00EE (D in bits 16-23, E in bits 0-7)
// A2 = HL (contiguous 16-bit value)

// Set GB flags for BIT instruction: Z from 68k Z flag, N=0, H=1, C unchanged
static void compile_bit_flags(struct code_block *block)
{
    // After btst, 68k Z flag is set if bit was 0
    // GB Z flag is bit 7, H flag is bit 5
    // We need: Z set if tested bit was 0, N=0, H=1, C unchanged (bit 4)

    // Capture 68k Z flag FIRST before other instructions clobber it
    // scc stores 0xff if condition true, 0x00 if false
    emit_scc(block, 0x07, REG_68K_D_SCRATCH_1);  // seq: set if Z=1
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80);  // mask to just Z position

    // Keep only C flag (bit 4)
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10);

    // Set H flag (bit 5)
    emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);

    // OR in the Z flag
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);
}

// BIT n, reg - test bit n of register
static void compile_bit_reg(struct code_block *block, int bit, int gb_reg)
{
    switch (gb_reg) {
    case 0: // B - high byte of D5 (bits 16-23)
        emit_btst_imm_dn(block, bit + 16, REG_68K_D_BC);
        break;
    case 1: // C - low byte of D5 (bits 0-7)
        emit_btst_imm_dn(block, bit, REG_68K_D_BC);
        break;
    case 2: // D - high byte of D6 (bits 16-23)
        emit_btst_imm_dn(block, bit + 16, REG_68K_D_DE);
        break;
    case 3: // E - low byte of D6 (bits 0-7)
        emit_btst_imm_dn(block, bit, REG_68K_D_DE);
        break;
    case 4: // H - high byte of HL
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_btst_imm_dn(block, bit + 8, REG_68K_D_SCRATCH_1);
        break;
    case 5: // L - low byte of HL
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_btst_imm_dn(block, bit, REG_68K_D_SCRATCH_1);
        break;
    case 6: // (HL) - memory indirect
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);  // result in D0
        emit_btst_imm_dn(block, bit, REG_68K_D_NEXT_PC);
        break;
    case 7: // A
        emit_btst_imm_dn(block, bit, REG_68K_D_A);
        break;
    }
    compile_bit_flags(block);
}

// RES n, reg - clear bit n of register
static void compile_res_reg(struct code_block *block, int bit, int gb_reg)
{
    switch (gb_reg) {
    case 0: // B
        emit_bclr_imm_dn(block, bit + 16, REG_68K_D_BC);
        break;
    case 1: // C
        emit_bclr_imm_dn(block, bit, REG_68K_D_BC);
        break;
    case 2: // D
        emit_bclr_imm_dn(block, bit + 16, REG_68K_D_DE);
        break;
    case 3: // E
        emit_bclr_imm_dn(block, bit, REG_68K_D_DE);
        break;
    case 4: // H - need to move from A2, modify, move back
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_bclr_imm_dn(block, bit + 8, REG_68K_D_SCRATCH_1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        break;
    case 5: // L
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_bclr_imm_dn(block, bit, REG_68K_D_SCRATCH_1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        break;
    case 6: // (HL) - read, modify, write
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);  // D0 = memory[HL]
        emit_bclr_imm_dn(block, bit, REG_68K_D_NEXT_PC);  // clear bit in D0
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);  // D1 = address
        compile_call_dmg_write_d0(block);  // write D2 to address D1
        break;
    case 7: // A
        emit_bclr_imm_dn(block, bit, REG_68K_D_A);
        break;
    }
}

// SET n, reg - set bit n of register
static void compile_set_reg(struct code_block *block, int bit, int gb_reg)
{
    switch (gb_reg) {
    case 0: // B
        emit_bset_imm_dn(block, bit + 16, REG_68K_D_BC);
        break;
    case 1: // C
        emit_bset_imm_dn(block, bit, REG_68K_D_BC);
        break;
    case 2: // D
        emit_bset_imm_dn(block, bit + 16, REG_68K_D_DE);
        break;
    case 3: // E
        emit_bset_imm_dn(block, bit, REG_68K_D_DE);
        break;
    case 4: // H
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_bset_imm_dn(block, bit + 8, REG_68K_D_SCRATCH_1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        break;
    case 5: // L
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        emit_bset_imm_dn(block, bit, REG_68K_D_SCRATCH_1);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        break;
    case 6: // (HL) - read, modify, write
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read_to_d0(block);  // D0 = memory[HL]
        emit_bset_imm_dn(block, bit, REG_68K_D_NEXT_PC);  // set bit in D0
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);  // D1 = address
        compile_call_dmg_write_d0(block);  // write D0 to address D1
        break;
    case 7: // A
        emit_bset_imm_dn(block, bit, REG_68K_D_A);
        break;
    }
}

int compile_cb_insn(struct code_block *block, uint8_t cb_op)
{
    int op = cb_op >> 6;
    int bit = (cb_op >> 3) & 0x7;
    int reg = cb_op & 0x7;

    switch (op) {
    case 0: // rotates/shifts - only swap is implemented for now
        if (cb_op == 0x37) {
            // swap a
            emit_ror_b_imm(block, 4, REG_68K_D_A);
            emit_cmp_b_imm_dn(block, REG_68K_D_A, 0);
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
            return 1;
        }
        return 0;  // other rotates/shifts not yet implemented

    case 1: // BIT
        compile_bit_reg(block, bit, reg);
        return 1;

    case 2: // RES
        compile_res_reg(block, bit, reg);
        return 1;

    case 3: // SET
        compile_set_reg(block, bit, reg);
        return 1;
    }

    return 0;
}
