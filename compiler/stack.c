#include "stack.h"
#include "emitters.h"
#include "interop.h"
#include "compiler.h"

// helper for reading GB memory during compilation
#define READ_BYTE(off) (ctx->read(ctx->dmg, src_address + (off)))

void compile_ld_sp_imm16(
    struct compile_ctx *ctx,
    struct code_block *block,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    uint16_t gb_sp = READ_BYTE(*src_ptr) | (READ_BYTE(*src_ptr + 1) << 8);
    *src_ptr += 2;

    // always store gb_sp to context
    emit_move_w_dn(block, REG_68K_D_SCRATCH_1, gb_sp);
    emit_move_w_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_GB_SP, REG_68K_A_CTX);

    // calculate mac pointer based on GB SP range
    // sp_adjust=0 means slow mode (SP outside WRAM/HRAM)
    if (ctx && gb_sp >= 0xc000 && gb_sp <= 0xdfff) {
        // WRAM: main_ram + (gb_sp - 0xC000)
        uint32_t addr = (uint32_t) ctx->wram_base + (gb_sp - 0xc000);
        int32_t sp_adjust = 0xc000 - (int32_t) ctx->wram_base;
        emit_movea_l_imm32(block, REG_68K_A_SP, addr);
        emit_move_l_dn(block, REG_68K_D_SCRATCH_1, sp_adjust);
        emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
    } else if (ctx && gb_sp >= 0xff80 && gb_sp <= 0xfffe) {
        // HRAM: zero_page + (gb_sp - 0xFF80)
        uint32_t addr = (uint32_t) ctx->hram_base + (gb_sp - 0xff80);
        int32_t sp_adjust = 0xff80 - (int32_t) ctx->hram_base;
        emit_movea_l_imm32(block, REG_68K_A_SP, addr);
        emit_move_l_dn(block, REG_68K_D_SCRATCH_1, sp_adjust);
        emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
    } else {
        // slow mode: sp_adjust = 0, A3 holds GB SP value (not a valid pointer)
        // This is useful for testing and allows ld hl, sp+N to work via gb_sp
        emit_movea_w_imm16(block, REG_68K_A_SP, gb_sp);
        emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
        emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
    }
}

int compile_stack_op(
    struct code_block *block,
    uint8_t op,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    switch (op) {
    case 0xc5: // push bc
        // SP -= 2 (both A3 and gb_sp)
        emit_subq_w_an(block, REG_68K_A_SP, 2);
        emit_subi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
        // Reconstruct BC into D1.w
        compile_bc_to_addr(block);
        // [SP] = low byte (C)
        emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
        // swap to get high byte
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        // [SP+1] = high byte (B)
        emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);
        return 1;

    case 0xd5: // push de
        // SP -= 2 (both A3 and gb_sp)
        emit_subq_w_an(block, REG_68K_A_SP, 2);
        emit_subi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
        // Reconstruct DE into D1.w
        compile_de_to_addr(block);
        // [SP] = low byte (E)
        emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
        // swap to get high byte
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        // [SP+1] = high byte (D)
        emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);
        return 1;

    case 0xe5: // push hl
        // SP -= 2 (both A3 and gb_sp)
        emit_subq_w_an(block, REG_68K_A_SP, 2);
        emit_subi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
        // Get HL into scratch
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
        // [SP] = low byte (L)
        emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
        // swap to get high byte
        emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
        // [SP+1] = high byte (H)
        emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);
        return 1;

    case 0xf5: // push af
        // SP -= 2 (both A3 and gb_sp)
        emit_subq_w_an(block, REG_68K_A_SP, 2);
        emit_subi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
        // [SP] = F (low byte - flags)
        emit_move_b_dn_ind_an(block, REG_68K_D_FLAGS, REG_68K_A_SP);
        // [SP+1] = A (high byte)
        emit_move_b_dn_disp_an(block, REG_68K_D_A, 1, REG_68K_A_SP);
        return 1;

    case 0xc1: // pop bc
        {
            size_t slow_pop, done;

            // Check if sp_adjust is 0 (slow mode)
            emit_tst_l_disp_an(block, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
            slow_pop = block->length;
            emit_beq_w(block, 0);  // branch to slow path

            // Fast path: use A3
            emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_addq_w_an(block, REG_68K_A_SP, 2);
            emit_addi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
            done = block->length;
            emit_bra_w(block, 0);

            // Slow path: use gb_sp and dmg_read
            block->code[slow_pop + 2] = (block->length - slow_pop - 2) >> 8;
            block->code[slow_pop + 3] = (block->length - slow_pop - 2) & 0xff;
            compile_slow_pop_to_d1(block);

            // Patch done branch
            block->code[done + 2] = (block->length - done - 2) >> 8;
            block->code[done + 3] = (block->length - done - 2) & 0xff;

            // Convert D1.w = 0xBBCC to 0x00BB00CC in BC
            emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_BC);  // C = low byte
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);  // D1.b = B
            emit_swap(block, REG_68K_D_BC);
            emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_BC);  // B = high byte
            emit_swap(block, REG_68K_D_BC);
        }
        return 1;

    case 0xd1: // pop de
        {
            size_t slow_pop, done;

            // Check if sp_adjust is 0 (slow mode)
            emit_tst_l_disp_an(block, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
            slow_pop = block->length;
            emit_beq_w(block, 0);  // branch to slow path

            // Fast path: use A3
            emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_addq_w_an(block, REG_68K_A_SP, 2);
            emit_addi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
            done = block->length;
            emit_bra_w(block, 0);

            // Slow path: use gb_sp and dmg_read
            block->code[slow_pop + 2] = (block->length - slow_pop - 2) >> 8;
            block->code[slow_pop + 3] = (block->length - slow_pop - 2) & 0xff;
            compile_slow_pop_to_d1(block);

            // Patch done branch
            block->code[done + 2] = (block->length - done - 2) >> 8;
            block->code[done + 3] = (block->length - done - 2) & 0xff;

            // Convert D1.w = 0xDDEE to 0x00DD00EE in DE
            emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_DE);  // E = low byte
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);  // D1.b = D
            emit_swap(block, REG_68K_D_DE);
            emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_DE);  // D = high byte
            emit_swap(block, REG_68K_D_DE);
        }
        return 1;

    case 0xe1: // pop hl
        {
            size_t slow_pop, done;

            // Check if sp_adjust is 0 (slow mode)
            emit_tst_l_disp_an(block, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
            slow_pop = block->length;
            emit_beq_w(block, 0);  // branch to slow path

            // Fast path: use A3
            emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
            emit_addq_w_an(block, REG_68K_A_SP, 2);
            emit_addi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
            done = block->length;
            emit_bra_w(block, 0);

            // Slow path: use gb_sp and dmg_read
            block->code[slow_pop + 2] = (block->length - slow_pop - 2) >> 8;
            block->code[slow_pop + 3] = (block->length - slow_pop - 2) & 0xff;
            compile_slow_pop_to_d1(block);

            // Patch done branch
            block->code[done + 2] = (block->length - done - 2) >> 8;
            block->code[done + 3] = (block->length - done - 2) & 0xff;

            // HL = D1.w
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
        }
        return 1;

    case 0xf1: // pop af
        {
            size_t slow_pop, done;

            // Check if sp_adjust is 0 (slow mode)
            emit_tst_l_disp_an(block, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
            slow_pop = block->length;
            emit_beq_w(block, 0);  // branch to slow path

            // Fast path: use A3 - sets A and F directly
            emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_A);  // A = [SP+1]
            emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_FLAGS);  // F = [SP]
            emit_addq_w_an(block, REG_68K_A_SP, 2);
            emit_addi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
            done = block->length;
            emit_bra_w(block, 0);

            // Slow path: use gb_sp and dmg_read
            block->code[slow_pop + 2] = (block->length - slow_pop - 2) >> 8;
            block->code[slow_pop + 3] = (block->length - slow_pop - 2) & 0xff;
            compile_slow_pop_to_d1(block);
            // D1.w = 0xAAFF, A = high byte, F = low byte
            emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);  // F = low
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);  // D1.b = A
            emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_A);  // A = high

            // Patch done branch (skips slow path D1 extraction)
            block->code[done + 2] = (block->length - done - 2) >> 8;
            block->code[done + 3] = (block->length - done - 2) & 0xff;
        }
        return 1;

    case 0xf8: // ld hl, sp+i8
        {
            int8_t offset = (int8_t)READ_BYTE(*src_ptr);
            uint8_t uoffset = (uint8_t)offset;
            (*src_ptr)++;

            // Load gb_sp from context
            emit_move_w_disp_an_dn(block, JIT_CTX_GB_SP, REG_68K_A_CTX, REG_68K_D_SCRATCH_0);

            // Compute HL = GB_SP + sign_extended(offset)
            if (offset > 0 && offset <= 8) {
                emit_addq_w_dn(block, REG_68K_D_SCRATCH_0, offset);
            } else if (offset < 0 && -offset <= 8) {
                emit_subq_w_dn(block, REG_68K_D_SCRATCH_0, -offset);
            } else if (offset != 0) {
                emit_move_w_dn(block, REG_68K_D_SCRATCH_1, offset);
                emit_add_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
            }

            // Store result in HL
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_0, REG_68K_A_HL);

            // C flag set if (SP.low + offset) overflows byte
            // nothing depends on this
            // emit_move_w_disp_an_dn(block, JIT_CTX_GB_SP, REG_68K_A_CTX, REG_68K_D_SCRATCH_1);
            // emit_addi_b_dn(block, REG_68K_D_SCRATCH_1, uoffset);  // D1.b += offset, sets C
            // emit_scc(block, 0x05, REG_68K_D_FLAGS);  // scs: D7 = 0xff if C
            // emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10);  // D7 = 0x10 if C (C position)
        }
        return 1;

    case 0xf9: // ld sp, hl
        {
            // Store HL to gb_sp
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
            emit_move_w_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_GB_SP, REG_68K_A_CTX);

            if (ctx && ctx->wram_base) {
                // Runtime range check for WRAM/HRAM
                // Check WRAM: $C000 <= HL < $E000
                size_t check_hram, slow_mode, done;
                int32_t sp_adjust_wram = 0xc000 - (int32_t) ctx->wram_base;
                int32_t sp_adjust_hram = 0xff80 - (int32_t) ctx->hram_base;

                // Check if high byte is in [$C0, $E0)
                emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
                emit_rol_w_8(block, REG_68K_D_SCRATCH_1);  // get high byte into low position
                emit_subi_b_dn(block, REG_68K_D_SCRATCH_1, 0xc0);  // high byte - $C0
                emit_cmp_b_imm_dn(block, REG_68K_D_SCRATCH_1, 0x20);  // < $20 means [$C0, $E0)
                check_hram = block->length;
                emit_bcc_w(block, 0);  // bcc = branch if carry clear (>= $20)

                // WRAM path: A3 = wram_base + (HL - $C000)
                // Must use ADDA.L because ADDA.W sign-extends, which breaks for HL >= $8000
                emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
                emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
                emit_movea_l_imm32(block, REG_68K_A_SP, (uint32_t) ctx->wram_base - 0xc000);
                emit_adda_l_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
                emit_move_l_dn(block, REG_68K_D_SCRATCH_1, sp_adjust_wram);
                emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
                done = block->length;
                emit_bra_w(block, 0);

                // Check HRAM: $FF80 <= HL < $FFFF
                block->code[check_hram + 2] = (block->length - check_hram - 2) >> 8;
                block->code[check_hram + 3] = (block->length - check_hram - 2) & 0xff;

                emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
                emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
                emit_cmp_b_imm_dn(block, REG_68K_D_SCRATCH_1, 0xff);  // high byte == $FF?
                slow_mode = block->length;
                emit_bne_w(block, 0);  // if not $FF, slow mode

                // HRAM path: A3 = hram_base + (HL - $FF80)
                // Must use ADDA.L because ADDA.W sign-extends, which breaks for HL >= $8000
                emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
                emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
                emit_movea_l_imm32(block, REG_68K_A_SP, (uint32_t) ctx->hram_base - 0xff80);
                emit_adda_l_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
                emit_move_l_dn(block, REG_68K_D_SCRATCH_1, sp_adjust_hram);
                emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
                emit_bra_b(block, 6);  // skip slow mode

                // Slow mode: sp_adjust = 0
                block->code[slow_mode + 2] = (block->length - slow_mode - 2) >> 8;
                block->code[slow_mode + 3] = (block->length - slow_mode - 2) & 0xff;
                emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
                emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);

                // Patch done branch
                block->code[done + 2] = (block->length - done - 2) >> 8;
                block->code[done + 3] = (block->length - done - 2) & 0xff;
            } else {
                // No context - simple path for testing
                emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
                emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
                emit_movea_l_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);
                emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
                emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_1, JIT_CTX_SP_ADJUST, REG_68K_A_CTX);
            }
        }
        return 1;

    default:
        return 0;
    }
}
