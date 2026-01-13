#include "compiler.h"
#include "emitters.h"

void compile_set_zc_flags(struct code_block *block)
{
    emit_move_sr_dn(block, REG_68K_D_FLAGS);
}

void compile_set_z_flag(struct code_block *block)
{
    emit_move_sr_dn(block, REG_68K_D_NEXT_PC);
    emit_andi_b_dn(block, REG_68K_D_NEXT_PC, 0x04);
    emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x04);
    emit_or_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_FLAGS);
    emit_move_dn_ccr(block, REG_68K_D_FLAGS);
}

void compile_set_c_flag(struct code_block *block)
{
    emit_move_sr_dn(block, REG_68K_D_NEXT_PC);
    emit_andi_b_dn(block, REG_68K_D_NEXT_PC, 0x01);
    emit_andi_b_dn(block, REG_68K_D_FLAGS, ~0x01);
    emit_or_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_FLAGS);
    emit_move_dn_ccr(block, REG_68K_D_FLAGS);
}