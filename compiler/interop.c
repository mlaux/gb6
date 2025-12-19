#include "compiler.h"
#include "emitters.h"
#include "interop.h"

// Call dmg_write(dmg, addr, val) - addr in D1, val in D4 (A register)
void compile_call_dmg_write(struct code_block *block)
{
    // Push args right-to-left: val, addr, dmg
    emit_push_w_dn(block, REG_68K_D_A);  // push value (A register = D4)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = write func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_write
    emit_addq_l_an(block, 7, 8);  // clean up stack (4 + 2 + 2 = 8 bytes)
}

// Call dmg_write(dmg, addr, val) - addr in D1
void compile_call_dmg_write_imm(struct code_block *block, uint8_t val)
{
    emit_push_w_imm(block, val);
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 8);
}

// Call dmg_read(dmg, addr) - addr in D1, result stays in D0 (scratch)
void compile_call_dmg_read_to_d0(struct code_block *block)
{
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);
    // Result stays in D0
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
