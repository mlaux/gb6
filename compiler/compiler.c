#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "emitters.h"
#include "branches.h"
#include "flags.h"
#include "interop.h"

// helper for reading GB memory during compilation
#define READ_BYTE(off) (ctx->read(ctx->dmg, src_address + (off)))

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

static void compile_ld_imm16_split(
    struct code_block *block,
    uint8_t reg,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    uint8_t lobyte = READ_BYTE(*src_ptr);
    uint8_t hibyte = READ_BYTE(*src_ptr + 1);
    *src_ptr += 2;
    emit_move_l_dn(block, reg, hibyte << 16);
    emit_move_w_dn(block, reg, lobyte);
}

static void compile_ld_imm16_contiguous(
    struct code_block *block,
    uint8_t reg,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    uint8_t lobyte = READ_BYTE(*src_ptr);
    uint8_t hibyte = READ_BYTE(*src_ptr + 1);
    *src_ptr += 2;
    emit_movea_w_imm16(block, reg, hibyte << 8 | lobyte);
}

void compile_ld_sp_imm16(
    struct compile_ctx *ctx,
    struct code_block *block,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    uint16_t gb_sp = READ_BYTE(*src_ptr) | (READ_BYTE(*src_ptr + 1) << 8);
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

struct code_block *compile_block(uint16_t src_address, struct compile_ctx *ctx)
{
    struct code_block *block;
    uint16_t src_ptr = 0;
    uint8_t op;
    int done = 0;

#ifdef DEBUG_COMPILE
    printf("compile_block: src_address=0x%04x\n", src_address);
    printf("  first bytes: %02x %02x %02x\n",
           READ_BYTE(0), READ_BYTE(1), READ_BYTE(2));
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
        // detect overflow of code block
        // could split loops across multiple blocks which is correct but slower
        if (block->length > sizeof(block->code) - 40) {
            emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
            emit_move_w_dn(block, REG_68K_D_NEXT_PC, src_address + src_ptr);
            emit_rts(block);
            break;
        }

        block->m68k_offsets[src_ptr] = block->length;
        op = READ_BYTE(src_ptr);
        src_ptr++;

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
            compile_ld_imm16_split(block, REG_68K_D_BC, ctx, src_address, &src_ptr);
            break;
        case 0x11: // ld de, imm16
            compile_ld_imm16_split(block, REG_68K_D_DE, ctx, src_address, &src_ptr);
            break;
        case 0x21: // ld hl, imm16
            compile_ld_imm16_contiguous(block, REG_68K_A_HL, ctx, src_address, &src_ptr);
            break;
        case 0x31: // ld sp, imm16
            compile_ld_sp_imm16(ctx, block, src_address, &src_ptr);
            break;

        case 0x32: // ld (hl-), a
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1); // D1.w = HL
            compile_call_dmg_write(block); // dmg_write(dmg, D1.w, A)
            emit_subq_w_an(block, REG_68K_A_HL, 1); // HL--
            break;

        case 0x23: // inc hl
            emit_addq_w_an(block, REG_68K_A_HL, 1);
            break;

        case 0x2a: // ld a, (hl+)
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1); // D1.w = HL
            compile_call_dmg_read(block); // dmg_read(dmg, D1.w) into A
            emit_addq_w_an(block, REG_68K_A_HL, 1); // HL++
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
            emit_move_b_dn(block, REG_68K_D_BC, READ_BYTE(src_ptr++));
            emit_swap(block, REG_68K_D_BC);
            break;
        
        case 0x0b: // dec bc
            emit_ext_w_dn(block, REG_68K_D_BC);
            emit_subq_l_dn(block, REG_68K_D_BC, 1);
            break;

        case 0x0c: // inc c
            emit_addq_b_dn(block, REG_68K_D_BC, 1);
            compile_set_z_flag(block);
            // clear N flag
            emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x40);
            break;

        case 0x0d: // dec c
            emit_subq_b_dn(block, REG_68K_D_BC, 1);
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // N flag
            break;

        case 0x0e: // ld c, imm8
            emit_move_b_dn(block, REG_68K_D_BC, READ_BYTE(src_ptr++));
            break;

        case 0x16: // ld d, imm8
            emit_swap(block, REG_68K_D_DE);
            emit_move_b_dn(block, REG_68K_D_DE, READ_BYTE(src_ptr++));
            emit_swap(block, REG_68K_D_DE);
            break;

        case 0x19: // add hl, de
            // Reconstruct DE into D1.w, add to HL
            compile_de_to_addr(block);  // D1.w = 0xDDEE
            emit_adda_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            // TODO: flags - clears N, sets H and C appropriately
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);  // keep Z, clear N, H, C for now
            break;

        case 0x1e: // ld e, imm8
            emit_move_b_dn(block, REG_68K_D_DE, READ_BYTE(src_ptr++));
            break;

        case 0x26: // ld h, imm8
            // move.w a0, d5
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
             // rol.w #8, d5
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            emit_move_b_dn(block, REG_68K_D_SCRATCH_1, READ_BYTE(src_ptr++));
            // ror.w #8, d5
            emit_ror_w_8(block, REG_68K_D_SCRATCH_1);
            // movea.w d5, a0
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0x2e: // ld l, imm8
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
            emit_move_b_dn(block, REG_68K_D_SCRATCH_1, READ_BYTE(src_ptr++));
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0x3e: // ld a, imm8
            emit_moveq_dn(block, REG_68K_D_A, READ_BYTE(src_ptr++));
            break;

        case 0x10: // stop - end of execution
            emit_move_l_dn(block, REG_68K_D_NEXT_PC, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;

        case 0x18: // jr disp8
            done = compile_jr(block, ctx, &src_ptr, src_address);
            break;

        case 0x20: // jr nz, disp8
            compile_jr_cond(block, ctx, &src_ptr, src_address, 7, 0);
            break;
        case 0x28: // jr z, disp8
            compile_jr_cond(block, ctx, &src_ptr, src_address, 7, 1);
            break;
        case 0x30: // jr nc, disp8
            compile_jr_cond(block, ctx, &src_ptr, src_address, 4, 0);
            break;
        case 0x38: // jr c, disp8
            compile_jr_cond(block, ctx, &src_ptr, src_address, 4, 1);
            break;

        case 0x3d: // dec a
            emit_subq_b_dn(block, REG_68K_D_A, 1);
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // D7 |= 0x40 (N flag)
            break;

        case 0x47: // ld b, a
            emit_swap(block, REG_68K_D_BC);
            emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_BC);
            emit_swap(block, REG_68K_D_BC);
            break;

        case 0x4f: // ld c, a
            emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_BC);
            break;

        case 0x56: // ld d, (hl)
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
            compile_call_dmg_read_to_d0(block);  // result in D0
            emit_swap(block, REG_68K_D_DE);
            emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_DE);  // D0 -> D
            emit_swap(block, REG_68K_D_DE);
            break;

        case 0x5e: // ld e, (hl)
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
            compile_call_dmg_read_to_d0(block);  // result in D0
            emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_DE);  // D0 -> E
            break;

        case 0x5f: // ld e, a
            emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_DE);
            break;

        case 0x78: // ld a, b
            emit_swap(block, REG_68K_D_BC);
            emit_move_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
            emit_swap(block, REG_68K_D_BC);
            break;

        case 0x79: // ld a, c
            emit_move_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
            break;

        case 0x87: // add a, a
            emit_add_b_dn_dn(block, REG_68K_D_A, REG_68K_D_A);
            compile_set_z_flag(block);
            // TODO: proper H and C flags
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);  // keep Z, clear N, H, C for now
            break;

        case 0xfe: // cp a, imm8
            emit_cmp_b_imm_dn(block, REG_68K_D_A, READ_BYTE(src_ptr++));
            compile_set_znc_flags(block);
            break;

        case 0xaf: // xor a, a - always results in 0, Z=1
            emit_moveq_dn(block, REG_68K_D_A, 0);
            emit_move_b_dn(block, REG_68K_D_FLAGS, 0x80); // Z=1, N=0, H=0, C=0
            break;

        case 0x2f: // cpl - complement A
            emit_not_b_dn(block, REG_68K_D_A);
            // CPL sets N=1, H=1, preserves Z and C
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x60);  // set N and H
            break;

        case 0xa1: // and a, c
            emit_and_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);  // keep Z, clear N, C
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);   // set H
            break;

        case 0xa7: // and a, a - set flags based on A
            emit_cmp_b_imm_dn(block, REG_68K_D_A, 0);
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);  // keep Z, clear N, C
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);   // set H
            break;

        case 0xa9: // xor a, c
            emit_eor_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);  // clear N, H, C
            break;

        case 0xb0: // or a, b
            emit_swap(block, REG_68K_D_BC);
            emit_or_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
            emit_swap(block, REG_68K_D_BC);
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);  // clear N, H, C
            break;

        case 0xb1: // or a, c
            emit_or_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_A);
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);  // clear N, H, C
            break;

        case 0xcb: // CB prefix
            {
                uint8_t cb_op = READ_BYTE(src_ptr++);
                switch (cb_op) {
                case 0x37: // swap a
                    emit_ror_b_imm(block, 4, REG_68K_D_A);
                    // set Z if result is 0, clear N, H, C
                    emit_cmp_b_imm_dn(block, REG_68K_D_A, 0);
                    compile_set_z_flag(block);
                    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x80);
                    break;
                default:
                    block->error = 1;
                    block->failed_opcode = 0xcb00 | cb_op;
                    block->failed_address = src_address + src_ptr - 2;
                    emit_move_l_dn(block, REG_68K_D_NEXT_PC, 0xffffffff);
                    emit_rts(block);
                    done = 1;
                    break;
                }
            }
            break;

        case 0xc0: // ret nz
            compile_ret_cond(block, 7, 0);
            break;

        case 0xc2: // jp nz, imm16
            compile_jp_cond(block, ctx, &src_ptr, src_address, 7, 0);
            break;

        case 0xc3: // jp imm16
            {
                uint16_t target = READ_BYTE(src_ptr) | (READ_BYTE(src_ptr + 1) << 8);
                src_ptr += 2;
                emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
                emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
                emit_rts(block);
                done = 1;
            }
            break;

        case 0xca: // jp z, imm16
            compile_jp_cond(block, ctx, &src_ptr, src_address, 7, 1);
            break;

        case 0xc8: // ret z
            compile_ret_cond(block, 7, 1);
            break;

        case 0xc9: // ret
            compile_ret(block);
            done = 1;
            break;

        case 0xcd: // call imm16
            compile_call_imm16(block, ctx, &src_ptr, src_address);
            done = 1;
            break;

        case 0xd0: // ret nc
            compile_ret_cond(block, 4, 0);
            break;

        case 0xd2: // jp nc, imm16
            compile_jp_cond(block, ctx, &src_ptr, src_address, 4, 0);
            break;

        case 0xd5: // push de
            // SP -= 2
            emit_subq_w_an(block, REG_68K_A_SP, 2);
            // Reconstruct DE into D1.w
            compile_de_to_addr(block);
            // [SP] = low byte (E)
            emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
            // swap to get high byte
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            // [SP+1] = high byte (D)
            emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);
            break;

        case 0xd8: // ret c
            compile_ret_cond(block, 4, 1);
            break;

        case 0xda: // jp c, imm16
            compile_jp_cond(block, ctx, &src_ptr, src_address, 4, 1);
            break;

        case 0xe0: // ld ($ff00 + u8), a
            {
                uint16_t addr = 0xff00 + READ_BYTE(src_ptr++);
                emit_move_w_dn(block, REG_68K_D_SCRATCH_1, addr);
                compile_call_dmg_write(block);
            }
            break;

        case 0xe1: // pop hl
            // D1 = [SP+1] (high byte)
            emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            // D1.b = [SP] (low byte)
            emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            // SP += 2
            emit_addq_w_an(block, REG_68K_A_SP, 2);
            // HL = D1.w
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0xe9: // jp (hl)
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_NEXT_PC);
            emit_rts(block);
            done = 1;
            break;

        case 0xef: // rst 28h - like call $0028
            compile_rst_n(block, 0x28, src_address + src_ptr);
            done = 1;
            break;

        case 0xe6: // and a, #nn
            emit_andi_b_dn(block, REG_68K_D_A, READ_BYTE(src_ptr++));
            compile_set_z_flag(block);
            emit_andi_b_dn(block, REG_68K_D_FLAGS, 0xa0);  // keep Z, clear N, C
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x20);   // set H
            break;

        case 0xe2: // ld ($ff00 + c), a
            emit_move_w_dn(block, REG_68K_D_SCRATCH_1, 0xff00);
            emit_or_b_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);  // D1.b |= C
            compile_call_dmg_write(block);
            break;

        case 0xf0: // ld a, ($ff00 + u8)
            {
                uint16_t addr = 0xff00 + READ_BYTE(src_ptr++);
                emit_move_w_dn(block, REG_68K_D_SCRATCH_1, addr);
                compile_call_dmg_read(block);
            }
            break;

        case 0xf3: // di
            compile_call_ei_di(block, 0);
            break;

        case 0xfb: // ei
            compile_call_ei_di(block, 1);
            break;

        case 0x36: // ld (hl), u8
            {
                uint8_t val = READ_BYTE(src_ptr++);
                emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);  // D1.w = HL
                compile_call_dmg_write_imm(block, val);
            }
            break;

        case 0xea: // ld (u16), a
            {
                uint16_t addr = READ_BYTE(src_ptr) | (READ_BYTE(src_ptr + 1) << 8);
                src_ptr += 2;
                emit_move_w_dn(block, REG_68K_D_SCRATCH_1, addr);
                compile_call_dmg_write(block);
            }
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
