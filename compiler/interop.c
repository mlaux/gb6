#include "compiler.h"
#include "emitters.h"
#include "interop.h"

// Call dmg_write(dmg, addr, val) - addr in D1, val in D4 (A register)
void compile_call_dmg_write(struct code_block *block)
{
    // Push args right-to-left: val, addr, dmg
    emit_push_b_dn(block, REG_68K_D_A);  // push value (A register = D4)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = write func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_write
    emit_addq_l_an(block, 7, 8);  // clean up stack (4 + 2 + 2 = 8 bytes)
}

// Call dmg_write(dmg, addr, val) - addr in D1
void compile_call_dmg_write_imm(struct code_block *block, uint8_t val)
{
    emit_push_b_imm(block, val);
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 8);
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D0
void compile_call_dmg_write_d0(struct code_block *block)
{
    emit_push_b_dn(block, REG_68K_D_NEXT_PC);  // push value (D0)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = write func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_write
    emit_addq_l_an(block, 7, 8);  // clean up stack (4 + 2 + 2 = 8 bytes)
}

// Offset of read_page array in struct dmg (after cpu, rom, lcd pointers)
#define DMG_READ_PAGE_OFFSET 12

// Call dmg_read(dmg, addr) - addr in D1, result stays in D0 (scratch)
// Inlines the page table lookup for fast path, calls dmg_read for slow path
void compile_call_dmg_read_to_d0(struct code_block *block)
{
    // Fast path sizes for branch calculation:
    // move.w d1,d0 (2) + andi.w (4) + move.b indexed (4) + bra.b (2) = 12 bytes
    // Slow path size:
    // push.w (2) + push.l (4) + movea.l (4) + jsr (2) + addq.l (2) = 14 bytes

    // movea.l JIT_CTX_DMG(a4), a0  - get dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_DMG, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);

    // move.w d1, d0  - copy address
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_NEXT_PC);

    // lsr.w #8, d0  - get page index (high byte)
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_NEXT_PC);

    // add.w d0, d0; add.w d0, d0  - multiply by 4 for pointer size
    emit_lsl_w_imm_dn(block, 2, REG_68K_D_NEXT_PC);

    // movea.l 12(a0,d0.w), a0  - load page pointer
    emit_movea_l_idx_an_an(block, DMG_READ_PAGE_OFFSET, REG_68K_A_SCRATCH_1,
                           REG_68K_D_NEXT_PC, REG_68K_A_SCRATCH_1);

    // move.l a0, d0  - test for NULL (sets Z flag)
    emit_move_l_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_NEXT_PC);

    // beq.b slow_path  - branch if page is NULL
    emit_beq_b(block, 12);

    // Fast path: read directly from page
    // move.w d1, d0  - get address again
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_NEXT_PC);

    // andi.w #$ff, d0  - mask to get offset within page
    emit_andi_w_dn(block, REG_68K_D_NEXT_PC, 0x00ff);

    // move.b (a0,d0.w), d0  - read byte from page
    emit_move_b_idx_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_NEXT_PC,
                          REG_68K_D_NEXT_PC);

    // bra.b done  - skip slow path
    emit_bra_b(block, 14);

    // Slow path: call dmg_read(dmg, address)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);

    // done: result in D0
}

// Call dmg_read(dmg, addr) - addr in D1, result goes to D4 (A register)
void compile_call_dmg_read(struct code_block *block)
{
    compile_call_dmg_read_to_d0(block);
    // Move result from D0 to D4 (A register)
    emit_move_b_dn_dn(block, 0, REG_68K_D_A);
}

void compile_call_ei_di(struct code_block *block, int enabled)
{
    // push enabled
    emit_moveq_dn(block, REG_68K_D_SCRATCH_1, (int8_t) enabled);
    // i actually have this as a 16-bit int for some reason
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    // push dmg pointer
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    // load address of function
    emit_movea_l_disp_an_an(block, JIT_CTX_EI_DI, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // call dmg_ei_di
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    // clean up stack
    emit_addq_l_an(block, 7, 6);
}
