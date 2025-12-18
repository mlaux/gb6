#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "emitters.h"
#include "branches.h"
#include "flags.h"

void compiler_init(void)
{
    // nothing for now
}

// Reconstruct BC from split format (0x00BB00CC) into D1.w as 0xBBCC
static void compile_bc_to_addr(struct code_block *block)
{
    emit_move_l_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);  // D1 = 0x00BB00CC
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_2);  // D2.w = 0x00CC
    emit_swap(block, REG_68K_D_SCRATCH_1);  // D1 = 0x00CC00BB
    emit_lsl_w_imm_dn(block, 8, REG_68K_D_SCRATCH_1);  // D1.w = 0xBB00
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_2, REG_68K_D_SCRATCH_1);  // D1.w = 0xBBCC
}

// Reconstruct DE from split format (0x00DD00EE) into D1.w as 0xDDEE
static void compile_de_to_addr(struct code_block *block)
{
    emit_move_l_dn_dn(block, REG_68K_D_DE, REG_68K_D_SCRATCH_1);  // D1 = 0x00DD00EE
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_2);  // D2.w = 0x00EE
    emit_swap(block, REG_68K_D_SCRATCH_1);  // D1 = 0x00EE00DD
    emit_lsl_w_imm_dn(block, 8, REG_68K_D_SCRATCH_1);  // D1.w = 0xDD00
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_2, REG_68K_D_SCRATCH_1);  // D1.w = 0xDDEE
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D4 (A register)
static void compile_call_dmg_write(struct code_block *block)
{
    // Push args right-to-left: val, addr, dmg
    emit_push_w_dn(block, REG_68K_D_A);  // push value (A register = D4)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = write func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_write
    emit_addq_l_an(block, 7, 8);  // clean up stack (4 + 2 + 2 = 8 bytes)
}

// Call dmg_read(dmg, addr) - addr in D1, result goes to D4 (A register)
static void compile_call_dmg_read(struct code_block *block)
{
    // Push args right-to-left: addr, dmg
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = read func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_read
    emit_addq_l_an(block, 7, 6);  // clean up stack (4 + 2 = 6 bytes)

    // Move result from D0 to D4 (A register)
    emit_move_b_dn_dn(block, 0, REG_68K_D_A);
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

void compile_ld_sp_imm16(
    struct compile_ctx *ctx,
    struct code_block *block,
    uint8_t *gb_code,
    uint16_t *src_ptr
) {
    uint16_t gb_sp = gb_code[*src_ptr] | (gb_code[*src_ptr + 1] << 8);
    *src_ptr += 2;

    // calculate mac pointer based on GB SP range
    if (ctx && gb_sp >= 0xc000 && gb_sp <= 0xdfff) {
        // WRAM: main_ram + (gb_sp - 0xC000)
        uint32_t addr = (uint32_t) ctx->wram_base + (gb_sp - 0xc000);
        emit_movea_l_imm32(block, REG_68K_A_SP, addr);
    } else if (ctx && gb_sp >= 0xff80 && gb_sp <= 0xfffe) {
        // HRAM: zero_page + (gb_sp - 0xFF80)
        uint32_t addr = (uint32_t) ctx->hram_base + (gb_sp - 0xff80);
        emit_movea_l_imm32(block, REG_68K_A_SP, addr);
    } else {
        // for testing
        emit_movea_w_imm16(block, REG_68K_A_SP, gb_sp);
    }
}

struct code_block *compile_block(
    uint16_t src_address,
    uint8_t *gb_code,
    struct compile_ctx *ctx
) {
    struct code_block *block;
    uint16_t src_ptr = 0;
    uint8_t op;
    int done = 0;

#ifdef DEBUG_COMPILE
    printf("compile_block: src_address=0x%04x, gb_code=%p\n",
           src_address, (void *)gb_code);
    printf("  first bytes: %02x %02x %02x\n",
           gb_code[0], gb_code[1], gb_code[2]);
#endif

    block = malloc(sizeof *block);
    if (!block) {
        return NULL;
    }

    block->length = 0;
    block->src_address = src_address;
    block->error = 0;
    block->failed_opcode = 0;
    block->failed_address = 0;

    while (!done) {
        block->m68k_offsets[src_ptr] = block->length;
        op = gb_code[src_ptr++];

        switch (op) {
        case 0x00: // nop
            // emit nothing
            break;

        case 0x02: // ld (bc), a
            compile_bc_to_addr(block);  // BC -> D1.w
            compile_call_dmg_write(block);  // dmg_write(dmg, D1.w, A)
            break;

        case 0x0a: // ld a, (bc)
            compile_bc_to_addr(block);  // BC -> D1.w
            compile_call_dmg_read(block);  // A = dmg_read(dmg, D1.w)
            break;

        case 0x12: // ld (de), a
            compile_de_to_addr(block);  // DE -> D1.w
            compile_call_dmg_write(block);  // dmg_write(dmg, D1.w, A)
            break;

        case 0x1a: // ld a, (de)
            compile_de_to_addr(block);  // DE -> D1.w
            compile_call_dmg_read(block);  // A = dmg_read(dmg, D1.w)
            break;

        case 0x01: // ld bc, imm16
            compile_ld_imm16_split(block, REG_68K_D_BC, gb_code, &src_ptr);
            break;
        case 0x11: // ld de, imm16
            compile_ld_imm16_split(block, REG_68K_D_DE, gb_code, &src_ptr);
            break;
        case 0x21: // ld hl, imm16
            compile_ld_imm16_contiguous(block, REG_68K_A_HL, gb_code, &src_ptr);
            break;
        case 0x31: // ld sp, imm16
            compile_ld_sp_imm16(ctx, block, gb_code, &src_ptr);
            break;

        case 0x32: // ld (hl-), a
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);  // D1.w = HL
            compile_call_dmg_write(block);  // dmg_write(dmg, D1.w, A)
            emit_subq_w_an(block, REG_68K_A_HL, 1);  // HL--
            break;

        case 0x05: // dec b
            emit_swap(block, REG_68K_D_BC);
            emit_subq_b_dn(block, REG_68K_D_BC, 1);
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // N flag
            emit_swap(block, REG_68K_D_BC);
            break;

        case 0x06: // ld b, imm8
            emit_swap(block, REG_68K_D_BC);
            emit_move_b_dn(block, REG_68K_D_BC, gb_code[src_ptr++]);
            emit_swap(block, REG_68K_D_BC);
            break;

        case 0x0d: // dec c
            emit_subq_b_dn(block, REG_68K_D_BC, 1);
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // N flag
            break;

        case 0x0e: // ld c, imm8
            emit_move_b_dn(block, REG_68K_D_BC, gb_code[src_ptr++]);
            break;

        case 0x16: // ld d, imm8
            emit_swap(block, REG_68K_D_DE);
            emit_move_b_dn(block, REG_68K_D_DE, gb_code[src_ptr++]);
            emit_swap(block, REG_68K_D_DE);
            break;

        case 0x1e: // ld e, imm8
            emit_move_b_dn(block, REG_68K_D_DE, gb_code[src_ptr++]);
            break;

        case 0x26: // ld h, imm8
            // move.w a0, d5
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
             // rol.w #8, d5
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            emit_move_b_dn(block, REG_68K_D_SCRATCH_1, gb_code[src_ptr++]);
            // ror.w #8, d5
            emit_ror_w_8(block, REG_68K_D_SCRATCH_1);
            // movea.w d5, a0
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0x2e: // ld l, imm8
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
            emit_move_b_dn(block, REG_68K_D_SCRATCH_1, gb_code[src_ptr++]);
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0x3e: // ld a, imm8
            emit_moveq_dn(block, REG_68K_D_A, gb_code[src_ptr++]);
            break;

        case 0x10: // stop - end of execution
            emit_move_l_dn(block, REG_68K_D_NEXT_PC, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;

        case 0x18: // jr disp8
            done = compile_jr(block, gb_code, &src_ptr, src_address);
            break;

        case 0x20: // jr nz, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 7, 0);
            break;
        case 0x28: // jr z, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 7, 1);
            break;
        case 0x30: // jr nc, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 4, 0);
            break;
        case 0x38: // jr c, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 4, 1);
            break;

        case 0x3d: // dec a
            emit_subq_b_dn(block, REG_68K_D_A, 1);
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // D7 |= 0x40 (N flag)
            break;

        case 0xfe: // cp a, imm8
            emit_cmp_b_imm_dn(block, REG_68K_D_A, gb_code[src_ptr++]);
            compile_set_znc_flags(block);
            break;

        case 0xaf: // xor a, a - always results in 0, Z=1
            emit_moveq_dn(block, REG_68K_D_A, 0);
            emit_move_b_dn(block, REG_68K_D_FLAGS, 0x80); // Z=1, N=0, H=0, C=0
            break;

        case 0xc3: // jp imm16
            {
                uint16_t target = gb_code[src_ptr] | (gb_code[src_ptr + 1] << 8);
                src_ptr += 2;
                emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
                emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
                emit_rts(block);
                done = 1;
            }
            break;

        case 0xc9: // ret
            compile_ret(block);
            done = 1;
            break;

        case 0xcd: // call imm16
            compile_call_imm16(block, gb_code, &src_ptr);
            done = 1;
            break;

        default:
            // unknown opcode - set error info and halt
            block->error = 1;
            block->failed_opcode = op;
            block->failed_address = src_address + src_ptr - 1;
            emit_move_l_dn(block, REG_68K_D_NEXT_PC, 0xffffffff);
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
