#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

void emit_byte(struct code_block *block, uint8_t byte)
{
    if (block->length < sizeof(block->code)) {
        block->code[block->length++] = byte;
    }
}

void emit_word(struct code_block *block, uint16_t word)
{
    emit_byte(block, word >> 8);
    emit_byte(block, word & 0xff);
}

void emit_long(struct code_block *block, uint32_t val)
{
    emit_byte(block, val >> 24);
    emit_byte(block, val >> 16);
    emit_byte(block, val >> 8);
    emit_byte(block, val & 0xff);
}

void emit_moveq_dn(struct code_block *block, uint8_t reg, int8_t imm)
{
    emit_byte(block, 0x70 | reg << 1);
    emit_byte(block, (uint8_t) imm);
}

// move.b #imm, Dn
void emit_move_b_dn(struct code_block *block, uint8_t reg, int8_t imm)
{
    // 01 = byte mode
    // dest effective address = rrr 000
    // src effective address = 111 100 means immediate
    uint16_t ins = (1 << 12) | (reg << 9) | (7 << 3) | 4;
    emit_word(block, ins);
    emit_word(block, (uint8_t) imm);
}

// move.w #imm, Dn
void emit_move_w_dn(struct code_block *block, uint8_t reg, int16_t imm)
{
    uint16_t ins = (3 << 12) | (reg << 9) | (7 << 3) | 4;
    emit_word(block, ins);
    emit_word(block, (uint16_t) imm);
}

// move.l #imm, Dn
void emit_move_l_dn(struct code_block *block, uint8_t reg, int32_t imm)
{
    uint16_t ins = (2 << 12) | (reg << 9) | (7 << 3) | 4;
    emit_word(block, ins);
    emit_long(block, (uint32_t) imm);
}

// rol.w #8, Dn
void emit_rol_w_8(struct code_block *block, uint8_t reg)
{
    // 1110 ccc d ss i 11 rrr
    // ccc=000 (8), d=1 (left), ss=01 (word), i=0 (immediate), rrr=reg
    emit_word(block, 0xe158 | reg);
}

// ror.w #8, Dn
void emit_ror_w_8(struct code_block *block, uint8_t reg)
{
    // ccc=000 (8), d=0 (right), ss=01 (word), i=0 (immediate), rrr=reg
    emit_word(block, 0xe058 | reg);
}

// swap Dn - exchange high and low 16-bit words
void emit_swap(struct code_block *block, uint8_t reg)
{
    // 0100 1000 0100 0 rrr
    emit_word(block, 0x4840 | reg);
}

// move.w An, Dn - copy address register to data register
void emit_move_w_an_dn(struct code_block *block, uint8_t areg, uint8_t dreg)
{
    // 00 11 ddd 000 001 aaa
    emit_word(block, 0x3008 | (dreg << 9) | areg);
}

// movea.w Dn, An - copy data register to address register
void emit_movea_w_dn_an(struct code_block *block, uint8_t dreg, uint8_t areg)
{
    // 00 11 aaa 001 000 ddd
    emit_word(block, 0x3040 | (areg << 9) | dreg);
}

void emit_movea_w_imm16(struct code_block *block, uint8_t areg, uint16_t val)
{
    // 00 11 aaa 001 111 100
    emit_word(block, 0x307c | areg << 9);
    emit_word(block, val);
}

// subq.b #val, Dn
void emit_subq_b_dn(struct code_block *block, uint8_t dreg, uint8_t val)
{
    uint16_t ddd;

    // 0101 ddd 1 00 000 rrr
    if (val == 0 || val > 8) {
        printf("can only subq values between 1 and 8\n");
        exit(1);
    }
    ddd = val == 8 ? 0 : val;
    emit_word(block, 0x5100 | ddd << 9 | dreg);
}

// subq.w #val, An
void emit_subq_w_an(struct code_block *block, uint8_t areg, uint8_t val)
{
    uint16_t ddd;

    // 0101 ddd 1 01 001 aaa
    if (val == 0 || val > 8) {
        printf("can only subq values between 1 and 8\n");
        exit(1);
    }
    ddd = val == 8 ? 0 : val;
    emit_word(block, 0x5148 | ddd << 9 | areg);
}

// addq.w #val, An
void emit_addq_w_an(struct code_block *block, uint8_t areg, uint8_t val)
{
    uint16_t ddd;

    // 0101 ddd 0 01 001 aaa
    if (val == 0 || val > 8) {
        printf("can only addq values between 1 and 8\n");
        exit(1);
    }
    ddd = val == 8 ? 0 : val;
    emit_word(block, 0x5048 | ddd << 9 | areg);
}

// move.w Dn, (An) - store data register to memory via address register
void emit_move_w_dn_ind_an(struct code_block *block, uint8_t dreg, uint8_t areg)
{
    // 00 11 aaa 010 000 ddd
    emit_word(block, 0x3080 | (areg << 9) | dreg);
}

// move.w (An), Dn - load from memory via address register to data register
void emit_move_w_ind_an_dn(struct code_block *block, uint8_t areg, uint8_t dreg)
{
    // 00 11 ddd 000 010 aaa
    emit_word(block, 0x3010 | (dreg << 9) | areg);
}

// move.b Dn, (An) - store byte to memory via address register
void emit_move_b_dn_ind_an(struct code_block *block, uint8_t dreg, uint8_t areg)
{
    // 00 01 aaa 010 000 ddd
    emit_word(block, 0x1080 | (areg << 9) | dreg);
}

// move.b Dn, d(An) - store byte to memory with displacement
void emit_move_b_dn_disp_an(struct code_block *block, uint8_t dreg, int16_t disp, uint8_t areg)
{
    // 00 01 aaa 101 000 ddd
    emit_word(block, 0x1140 | (areg << 9) | dreg);
    emit_word(block, disp);
}

// move.b (An), Dn - load byte from memory via address register
void emit_move_b_ind_an_dn(struct code_block *block, uint8_t areg, uint8_t dreg)
{
    // 00 01 ddd 000 010 aaa
    emit_word(block, 0x1010 | (dreg << 9) | areg);
}

// move.b d(An), Dn - load byte from memory with displacement
void emit_move_b_disp_an_dn(struct code_block *block, int16_t disp, uint8_t areg, uint8_t dreg)
{
    // 00 01 ddd 000 101 aaa
    emit_word(block, 0x1028 | (dreg << 9) | areg);
    emit_word(block, disp);
}

// andi.l #imm, Dn
void emit_andi_l_dn(struct code_block *block, uint8_t dreg, uint32_t imm)
{
    // 0000 0010 10 000 rrr
    emit_word(block, 0x0280 | dreg);
    emit_long(block, imm);
}

// cmp.b #imm, Dn
void emit_cmp_b_imm_dn(struct code_block *block, uint8_t dreg, uint8_t imm)
{
    // 0000 1100 00 000 rrr
    emit_word(block, 0x0c00 | dreg);
    emit_word(block, imm);
}

// scc Dn - set byte based on condition
// cond: 0x7 = seq (Z=1), 0x6 = sne (Z=0), 0x5 = scs (C=1), 0x4 = scc (C=0)
void emit_scc(struct code_block *block, uint8_t cond, uint8_t dreg)
{
    // 0101 cccc 11 000 rrr
    emit_word(block, 0x50c0 | (cond << 8) | dreg);
}

// andi.b #imm, Dn
void emit_andi_b_dn(struct code_block *block, uint8_t dreg, uint8_t imm)
{
    // 0000 0010 00 000 rrr
    emit_word(block, 0x0200 | dreg);
    emit_word(block, imm);
}

// ori.b #imm, Dn
void emit_ori_b_dn(struct code_block *block, uint8_t dreg, uint8_t imm)
{
    // 0000 0000 00 000 rrr
    emit_word(block, 0x0000 | dreg);
    emit_word(block, imm);
}

// or.b Ds, Dd (result to Dd)
void emit_or_b_dn_dn(struct code_block *block, uint8_t src, uint8_t dest)
{
    // 1000 ddd 0 00 000 sss (direction bit 0 = result to Dn)
    emit_word(block, 0x8000 | (dest << 9) | src);
}

// Emit: rts
void emit_rts(struct code_block *block)
{
    emit_word(block, 0x4e75);
}

// bra.w - branch always with 16-bit displacement
void emit_bra_w(struct code_block *block, int16_t disp)
{
    emit_word(block, 0x6000);
    emit_word(block, disp);
}

// beq.w - branch if Z=1 with 16-bit displacement
void emit_beq_w(struct code_block *block, int16_t disp)
{
    emit_word(block, 0x6700);
    emit_word(block, disp);
}

// bne.w - branch if Z=0 with 16-bit displacement
void emit_bne_w(struct code_block *block, int16_t disp)
{
    emit_word(block, 0x6600);
    emit_word(block, disp);
}

// btst #bit, Dn - test bit, sets Z=1 if bit is 0
void emit_btst_imm_dn(struct code_block *block, uint8_t bit, uint8_t dreg)
{
    // 0000 1000 00 000 rrr, followed by bit number
    emit_word(block, 0x0800 | dreg);
    emit_word(block, bit);
}
