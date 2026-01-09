#include "compiler.h"
#include "emitters.h"

// can remove these now
void compile_set_zc_flags(struct code_block *block)
{
    emit_move_sr_dn(block, REG_68K_D_FLAGS);
}

void compile_set_z_flag(struct code_block *block)
{
    emit_move_sr_dn(block, REG_68K_D_FLAGS);
}
