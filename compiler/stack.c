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

    // A3 holds the GB SP value directly (not a native pointer)
    emit_movea_w_imm16(block, REG_68K_A_SP, gb_sp);
}

// Push D3.w to stack via dmg_write. Clobbers D0, D1, D3.
static void compile_push_d3(struct code_block *block)
{
    // SP -= 2
    emit_subq_w_an(block, REG_68K_A_SP, 2);

    // Write low byte: dmg_write(SP, D3.b)
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
    emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_0);
    compile_call_dmg_write_d0(block);

    // Write high byte: dmg_write(SP+1, D3.b >> 8)
    emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
    emit_addq_w_dn(block, REG_68K_D_SCRATCH_1, 1);
    emit_rol_w_8(block, REG_68K_D_NEXT_PC);
    emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_0);
    compile_call_dmg_write_d0(block);
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

            // Write low byte of SP
            emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
            emit_move_w_dn(block, REG_68K_D_SCRATCH_1, addr);
            compile_call_dmg_write_d0(block);

            // Write high byte of SP
            emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_0);
            emit_rol_w_8(block, REG_68K_D_SCRATCH_0);
            emit_move_w_dn(block, REG_68K_D_SCRATCH_1, addr + 1);
            compile_call_dmg_write_d0(block);
        }
        return 1;

    case 0xc5: // push bc
        compile_bc_to_addr(block);  // D1.w = BC
        emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_NEXT_PC);
        compile_push_d3(block);
        return 1;

    case 0xd5: // push de
        compile_de_to_addr(block);  // D1.w = DE
        emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_NEXT_PC);
        compile_push_d3(block);
        return 1;

    case 0xe5: // push hl
        emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_NEXT_PC);
        compile_push_d3(block);
        return 1;

    case 0xf5: // push af
        // Build AF in D3: A in high byte, F in low byte
        emit_move_b_dn_dn(block, REG_68K_D_A, REG_68K_D_NEXT_PC);
        emit_rol_w_8(block, REG_68K_D_NEXT_PC);
        emit_move_b_dn_dn(block, REG_68K_D_FLAGS, REG_68K_D_NEXT_PC);
        compile_push_d3(block);
        return 1;

    case 0xc1: // pop bc
        // Read low byte from [SP]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read(block);  // D0 = low byte
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_BC);  // C = low byte

        // Read high byte from [SP+1]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        emit_addq_w_dn(block, REG_68K_D_SCRATCH_1, 1);
        compile_call_dmg_read(block);  // D0 = high byte
        emit_swap(block, REG_68K_D_BC);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_BC);  // B = high byte
        emit_swap(block, REG_68K_D_BC);

        // SP += 2
        emit_addq_w_an(block, REG_68K_A_SP, 2);
        return 1;

    case 0xd1: // pop de
        // Read low byte from [SP]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read(block);  // D0 = low byte
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_DE);  // E = low byte

        // Read high byte from [SP+1]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        emit_addq_w_dn(block, REG_68K_D_SCRATCH_1, 1);
        compile_call_dmg_read(block);  // D0 = high byte
        emit_swap(block, REG_68K_D_DE);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_DE);  // D = high byte
        emit_swap(block, REG_68K_D_DE);

        // SP += 2
        emit_addq_w_an(block, REG_68K_A_SP, 2);
        return 1;

    case 0xe1: // pop hl
        // Read low byte from [SP]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read(block);  // D0 = low byte
        emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_NEXT_PC);  // save low in D3

        // Read high byte from [SP+1]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        emit_addq_w_dn(block, REG_68K_D_SCRATCH_1, 1);
        compile_call_dmg_read(block);  // D0 = high byte

        // Combine: D0.b = high, D3.b = low -> HL = (high << 8) | low
        emit_rol_w_8(block, REG_68K_D_SCRATCH_0);
        emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_0);
        emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_0, REG_68K_A_HL);

        // SP += 2
        emit_addq_w_an(block, REG_68K_A_SP, 2);
        return 1;

    case 0xf1: // pop af
        // Read F (low byte) from [SP]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        compile_call_dmg_read(block);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_FLAGS);

        // Read A (high byte) from [SP+1]
        emit_move_w_an_dn(block, REG_68K_A_SP, REG_68K_D_SCRATCH_1);
        emit_addq_w_dn(block, REG_68K_D_SCRATCH_1, 1);
        compile_call_dmg_read(block);
        emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_A);

        // SP += 2
        emit_addq_w_an(block, REG_68K_A_SP, 2);
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
