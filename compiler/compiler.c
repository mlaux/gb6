#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>

void compiler_init(void)
{
    // nothing for now
}

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

static void emit_moveq_dn(struct code_block *block, uint8_t reg, int8_t imm)
{
    emit_byte(block, 0x70 | reg << 1);
    emit_byte(block, (uint8_t)imm);
}

// move.b #imm, Dn
static void emit_move_b_dn(struct code_block *block, uint8_t reg, int8_t imm)
{
    // 01 = byte mode
    // dest effective address = rrr 000
    // src effective address = 111 100 means immediate
    uint16_t ins = (1 << 12) | (reg << 9) | (7 << 3) | 4;
    emit_word(block, ins);
    emit_word(block, (uint8_t) imm);
}

// move.w #imm, Dn
static void emit_move_w_dn(struct code_block *block, uint8_t reg, int16_t imm)
{
    uint16_t ins = (3 << 12) | (reg << 9) | (7 << 3) | 4;
    emit_word(block, ins);
    emit_word(block, (uint16_t) imm);
}

// move.l #imm, Dn
static void emit_move_l_dn(struct code_block *block, uint8_t reg, int32_t imm)
{
    uint16_t ins = (2 << 12) | (reg << 9) | (7 << 3) | 4;
    emit_word(block, ins);
    emit_long(block, (uint32_t) imm);
}

// rol.w #8, Dn
static void emit_rol_w_8(struct code_block *block, uint8_t reg)
{
    // 1110 ccc d ss i 11 rrr
    // ccc=000 (8), d=1 (left), ss=01 (word), i=0 (immediate), rrr=reg
    emit_word(block, 0xe158 | reg);
}

// ror.w #8, Dn
static void emit_ror_w_8(struct code_block *block, uint8_t reg)
{
    // ccc=000 (8), d=0 (right), ss=01 (word), i=0 (immediate), rrr=reg
    emit_word(block, 0xe058 | reg);
}

// swap Dn - exchange high and low 16-bit words (4 cycles vs 22 for rol #8)
static void emit_swap(struct code_block *block, uint8_t reg)
{
    // 0100 1000 0100 0 rrr
    emit_word(block, 0x4840 | reg);
}

// move.w An, Dn - copy address register to data register
static void emit_move_w_an_dn(struct code_block *block, uint8_t areg, uint8_t dreg)
{
    // 00 11 ddd 000 001 aaa
    emit_word(block, 0x3008 | (dreg << 9) | areg);
}

// movea.w Dn, An - copy data register to address register
static void emit_movea_w_dn_an(struct code_block *block, uint8_t dreg, uint8_t areg)
{
    // 00 11 aaa 001 000 ddd
    emit_word(block, 0x3040 | (areg << 9) | dreg);
}

static void emit_movea_w_imm16(struct code_block *block, uint8_t areg, uint16_t val)
{
    // 00 11 aaa 001 111 100
    emit_word(block, 0x307c | areg << 9);
    emit_word(block, val);
}

// subq.b #, Dn
static void emit_subq_b_1_dn(struct code_block *block, uint8_t dreg, uint8_t val)
{
    // 0101 ddd 1 ss 000 rrr
    // ss = 00
    // rrr = dreg
    if (val == 0 || val > 8) {
        printf("can only subq values between 1 and 8\n");
        exit(1);
    }
    uint16_t ddd = val == 8 ? 0 : val;
    emit_word(block, 0x5300 | ddd << 9 | dreg);
}

// Emit: rts
static void emit_rts(struct code_block *block)
{
    emit_word(block, 0x4e75);
}

static void compile_ld_imm16_split(
    struct code_block *block,
    uint8_t reg,
    uint8_t *gb_code,
    uint16_t *src_ptr
) {
    uint8_t lobyte = gb_code[*src_ptr],
            hibyte = gb_code[*src_ptr + 1];
    *src_ptr += 2;
    emit_move_l_dn(block, reg, hibyte << 16);
    emit_move_w_dn(block, reg, lobyte);
}

static void compile_ld_imm16_contiguous(
    struct code_block *block,
    uint8_t reg,
    uint8_t *gb_code,
    uint16_t *src_ptr
) {
    uint8_t lobyte = gb_code[*src_ptr],
            hibyte = gb_code[*src_ptr + 1];
    *src_ptr += 2;
    emit_movea_w_imm16(block, reg, hibyte << 8 | lobyte);
}

// bra.w - branch always with 16-bit displacement
static void emit_bra_w(struct code_block *block, int16_t disp)
{
    emit_word(block, 0x6000);
    emit_word(block, disp);
}

// returns 1 if jr ended the block, 0 if it's a backward jump within block
static int compile_jr(
    struct code_block *block,
    uint8_t *gb_code,
    uint16_t *src_ptr,
    uint16_t src_address
) {
    int8_t disp = (int8_t) gb_code[*src_ptr];
    (*src_ptr)++;

    // jr displacement is relative to PC after the jr instruction
    // *src_ptr now points to the byte after jr, so target = *src_ptr + disp
    int16_t target_gb_offset = (int16_t) *src_ptr + disp;

    // Check if this is a backward jump to a location we've already compiled
    if (target_gb_offset >= 0 && target_gb_offset < (int16_t) (*src_ptr - 2)) {
        // Backward jump within block - emit bra.w
        uint16_t target_m68k = block->m68k_offsets[target_gb_offset];
        // bra.w displacement is relative to PC after the opcode word (before extension)
        // current position = block->length, PC after opcode = block->length + 2
        int16_t m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);
        emit_bra_w(block, m68k_disp);
        return 0;
    }

    // Forward jump or outside block - go through dispatcher
    uint16_t target_gb_pc = src_address + target_gb_offset;
    emit_moveq_dn(block, 4, 0);
    emit_move_w_dn(block, 4, target_gb_pc);
    emit_rts(block);
    return 1;
}

struct code_block *compile_block(uint16_t src_address, uint8_t *gb_code)
{
    struct code_block *block;
    uint16_t src_ptr = 0;
    uint8_t op;
    int done = 0;

    block = malloc(sizeof *block);
    if (!block) {
        return NULL;
    }

    block->length = 0;
    block->src_address = src_address;

    while (!done) {
        block->m68k_offsets[src_ptr] = block->length;
        op = gb_code[src_ptr++];

        switch (op) {
        case 0x00: // nop
            // emit nothing
            break;

        case 0x01: // ld bc, imm16
            compile_ld_imm16_split(block, 1, gb_code, &src_ptr);
            break;
        case 0x11: // ld de, imm16
            compile_ld_imm16_split(block, 2, gb_code, &src_ptr);
            break;
        case 0x21: // ld hl, imm16
            compile_ld_imm16_contiguous(block, 0, gb_code, &src_ptr);
            break;
        case 0x31: // ld sp, imm16
            compile_ld_imm16_contiguous(block, 1, gb_code, &src_ptr);
            break;

        case 0x0e:
            emit_move_b_dn(block, 1, gb_code[src_ptr++]);
            break;
        case 0x1e:
            emit_move_b_dn(block, 2, gb_code[src_ptr++]);
            break;

        case 0x06: // ld b, imm8 (high byte of BC/D1)
            emit_swap(block, 1);
            emit_move_b_dn(block, 1, gb_code[src_ptr++]);
            emit_swap(block, 1);
            break;

        case 0x16: // ld d, imm8 (high byte of DE/D2)
            emit_swap(block, 2);
            emit_move_b_dn(block, 2, gb_code[src_ptr++]);
            emit_swap(block, 2);
            break;

        case 0x26: // ld h, imm8
            emit_move_w_an_dn(block, 0, 3);      // move.w a0, d3
            emit_rol_w_8(block, 3);              // rol.w #8, d3
            emit_move_b_dn(block, 3, gb_code[src_ptr++]);
            emit_ror_w_8(block, 3);              // ror.w #8, d3
            emit_movea_w_dn_an(block, 3, 0);     // movea.w d3, a0
            break;

        case 0x2e: // ld l, imm8
            emit_move_w_an_dn(block, 0, 3);      // move.w a0, d3
            emit_move_b_dn(block, 3, gb_code[src_ptr++]);
            emit_movea_w_dn_an(block, 3, 0);     // movea.w d3, a0
            break;

        case 0x3e: // ld a, imm8
            emit_moveq_dn(block, 0, gb_code[src_ptr++]);
            break;

        case 0x10: // stop - end of execution
            emit_move_l_dn(block, 4, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;

        case 0x18: // jr disp8
            done = compile_jr(block, gb_code, &src_ptr, src_address);
            break;

        case 0x3d: // dec a
            emit_subq_b_1_dn(block, 0, 1);
            break;

        case 0xc3: // jp imm16
            {
                uint16_t target = gb_code[src_ptr] | (gb_code[src_ptr + 1] << 8);
                src_ptr += 2;
                emit_moveq_dn(block, 4, 0);
                emit_move_w_dn(block, 4, target);
                emit_rts(block);
                done = 1;
            }
            break;

        case 0xc9: // ret
            // TODO: implement proper GB stack pop
            emit_move_l_dn(block, 4, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;

        default:
            // call interpreter as a fallback?
            fprintf(stderr, "compile_block: unknown opcode 0x%02x at 0x%04x\n",
                    op, src_address + src_ptr - 1);
            emit_move_l_dn(block, 4, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;
        }
    }

    block->end_address = src_address + src_ptr;
    return block;
}

void block_free(struct code_block *block)
{
    free(block);
}
