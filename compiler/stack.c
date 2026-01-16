#include "stack.h"
#include "emitters.h"
#include "interop.h"
#include "compiler.h"

#define READ_BYTE(off) (ctx->read(ctx->dmg, src_address + (off)))

void compile_ld_sp_imm16(
    struct compile_ctx *ctx,
    struct code_block *block,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    uint16_t gb_sp = READ_BYTE(*src_ptr) | (READ_BYTE(*src_ptr + 1) << 8);
    *src_ptr += 2;

    emit_movea_w_imm16(block, REG_68K_A_SP, gb_sp);
}

// Push D0.w to stack with WRAM fast path. Clobbers D0, D1, D3.
static void compile_push_d0(struct code_block *block)
{
    // Save value to D3 before we clobber D0
    // move.w d0, d3                     ; 2 bytes [0-1]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_NEXT_PC);

    // Check if SP-2 in WRAM: ((SP-2) & 0xE000) == 0xC000
    // move.w a3, d0                     ; 2 bytes [2-3]
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
    // subq.w #2, d0                     ; 2 bytes [4-5]
    emit_subq_w_dn(block, REG_68K_D_SCRATCH_0, 2);
    // andi.w #$e000, d0                 ; 4 bytes [6-9]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0xe000);
    // cmpi.w #$c000, d0                 ; 4 bytes [10-13]
    emit_cmpi_w_imm_dn(block, 0xc000, REG_68K_D_SCRATCH_0);
    // bne.b slow_path (+24)             ; 2 bytes [14-15] -> offset 40
    emit_bne_b(block, 24);

    // WRAM fast path (offset 16)
    // subq.w #2, a3                     ; 2 bytes [16-17]
    emit_subq_w_an(block, REG_68K_A_SP, 2);
    // movea.l JIT_CTX_WRAM_BASE(a4), a0 ; 4 bytes [18-21]
    emit_movea_l_disp_an_an(block, JIT_CTX_WRAM_BASE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // move.w a3, d0                     ; 2 bytes [22-23]
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
    // andi.w #$1fff, d0                 ; 4 bytes [24-27]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x1fff);
    // move.b d3, (a0,d0.w)              ; 4 bytes [28-31] - low byte
    emit_move_b_dn_idx_an(block, REG_68K_D_NEXT_PC, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // lsr.w #8, d3                      ; 2 bytes [32-33]
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_NEXT_PC);
    // move.b d3, 1(a0,d0.w)             ; 4 bytes [34-37] - high byte
    emit_move_b_dn_disp_idx_an(block, REG_68K_D_NEXT_PC, 1, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // bra.b done (+30)                  ; 2 bytes [38-39] -> offset 70
    emit_bra_b(block, 30);

    // slow_path: (offset 40)
    // subq.w #2, a3                     ; 2 bytes [40-41]
    emit_subq_w_an(block, REG_68K_A_SP, 2);
    // move.w a3, d1                     ; 2 bytes [42-43]
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
    // move.w d3, d0                     ; 2 bytes [44-45]
    emit_move_w_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_0);
    // compile_slow_dmg_write16          ; 24 bytes [46-69]
    compile_slow_dmg_write16(block);
    // done: (offset 70)
}

// Pop 16-bit value from stack to D0 with WRAM fast path. Clobbers D0, D1.
static void compile_pop_to_d0(struct code_block *block)
{
    // Check if SP in WRAM: (SP & 0xE000) == 0xC000
    // move.w a3, d0                     ; 2 bytes [0-1]
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
    // andi.w #$e000, d0                 ; 4 bytes [2-5]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0xe000);
    // cmpi.w #$c000, d0                 ; 4 bytes [6-9]
    emit_cmpi_w_imm_dn(block, 0xc000, REG_68K_D_SCRATCH_0);
    // bne.b slow_path (+26)             ; 2 bytes [10-11] -> offset 38
    emit_bne_b(block, 26);

    // WRAM fast path (offset 12)
    // movea.l JIT_CTX_WRAM_BASE(a4), a0 ; 4 bytes [12-15]
    emit_movea_l_disp_an_an(block, JIT_CTX_WRAM_BASE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // move.w a3, d0                     ; 2 bytes [16-17]
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
    // andi.w #$1fff, d0                 ; 4 bytes [18-21]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x1fff);
    // move.b 1(a0,d0.w), d1             ; 4 bytes [22-25] - high byte
    emit_move_b_disp_idx_an_dn(block, 1, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_1);
    // lsl.w #8, d1                      ; 2 bytes [26-27]
    emit_lsl_w_imm_dn(block, 8, REG_68K_D_SCRATCH_1);
    // move.b (a0,d0.w), d1              ; 4 bytes [28-31] - low byte
    emit_move_b_idx_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes [32-33] - result to d0
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // addq.w #2, a3                     ; 2 bytes [34-35]
    emit_addq_w_an(block, REG_68K_A_SP, 2);
    // bra.b done (+26)                  ; 2 bytes [36-37] -> offset 64
    emit_bra_b(block, 26);

    // slow_path: (offset 38)
    // move.w a3, d1                     ; 2 bytes [38-39]
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
    // compile_slow_dmg_read16           ; 22 bytes [40-61]
    compile_slow_dmg_read16(block);
    // addq.w #2, a3                     ; 2 bytes [62-63]
    emit_addq_w_an(block, REG_68K_A_SP, 2);
    // done: (offset 64) - result in d0.w
}

int compile_stack_op(
    struct code_block *block,
    uint8_t op,
    struct compile_ctx *ctx,
    uint16_t src_address,
    uint16_t *src_ptr
) {
    switch (op) {
    case 0x08: // ld (u16), sp
        {
            uint16_t addr = READ_BYTE(*src_ptr) | (READ_BYTE(*src_ptr + 1) << 8);
            *src_ptr += 2;

            // dmg_write16(addr, SP)
            emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
            emit_move_w_dn(block, REG_68K_D_SCRATCH_1, addr);
            compile_call_dmg_write16_d0(block);
        }
        return 1;

    case 0xc5: // push bc
        compile_join_bc(block, REG_68K_D_SCRATCH_0);
        compile_push_d0(block);
        return 1;

    case 0xd5: // push de
        compile_join_de(block, REG_68K_D_SCRATCH_0);
        compile_push_d0(block);
        return 1;

    case 0xe5: // push hl
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_0);
        compile_push_d0(block);
        return 1;

    case 0xf5: // push af
        // Build AF in D0: A in high byte, F in low byte
        emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_SCRATCH_0);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_0);
        emit_move_b_dn_dn(block, REG_68K_D_FLAGS, REG_68K_D_SCRATCH_0);
        compile_push_d0(block);
        return 1;

    case 0xc1: // pop bc
        compile_pop_to_d0(block);

        // extract to split BC format
        emit_move_l_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_BC); // D5 = ????BBCC
        emit_lsl_l_imm_dn(block, 8, REG_68K_D_BC); // D5 = ??BBCC00
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_BC); // D5 = ??BBCCCC
        return 1;

    case 0xd1: // pop de
        compile_pop_to_d0(block);

        // extract to split DE format
        emit_move_l_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_DE);
        emit_lsl_l_imm_dn(block, 8, REG_68K_D_DE);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_DE);
        return 1;

    case 0xe1: // pop hl
        compile_pop_to_d0(block);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_0, REG_68K_A_HL);
        return 1;

    case 0xf1: // pop af
        compile_pop_to_d0(block);

        // Extract F (low byte) and A (high byte)
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_FLAGS);
        emit_rol_w_8(block, REG_68K_D_SCRATCH_0);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);
        return 1;

    case 0xe8: // add sp, i8
        {
            int8_t offset = (int8_t) READ_BYTE(*src_ptr);
            (*src_ptr)++;

            if (offset != 0) {
                emit_lea_disp_an_an(block, offset, REG_68K_A_SP, REG_68K_A_SP);
            }
        }
        return 1;

    case 0xf8: // ld hl, sp+i8
        {
            int8_t offset = (int8_t) READ_BYTE(*src_ptr);
            (*src_ptr)++;

            // Get SP into D0
            emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);

            // Compute HL = SP + sign_extended(offset)
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
        }
        return 1;

    case 0xf9: // ld sp, hl
        // Just copy HL to A3
        emit_movea_w_an_an(block, REG_68K_A_HL, REG_68K_A_SP);
        return 1;

    default:
        return 0;
    }
}
