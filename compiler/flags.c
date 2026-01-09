#include "compiler.h"
#include "emitters.h"

// need to optimize this - instead of having my D7 be ZNHC0000, can just
// have them in the same order as the 68k flags register, then `move ccr, d7`
// then don't need seq, scs, and when testing a condition, can just move d7 back into ccr

/*move.w sr, d0; move.b d0, d7*/


// Set Z and C flags in D7 based on current 68k CCR, N=0 (for add)
void compile_set_zc_flags(struct code_block *block)
{
    // extract both flags using scc (doesn't affect CCR)
    emit_scc(block, 0x7, REG_68K_D_SCRATCH_1);  // seq D1 (Z)
    emit_scc(block, 0x5, REG_68K_D_FLAGS);      // scs D7 (C)

    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80);  // D1 = 0x80 if Z
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10);      // D7 = 0x10 if C
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);  // D7 = Z | C
    // N=0, H=0 (TODO: H flag)
}

// Set Z flag in D7 based on current 68k CCR, preserving other flags
void compile_set_z_flag(struct code_block *block)
{
    // seq D1 (D1 = 0xff if Z, 0x00 if NZ)
    emit_scc(block, 0x7, REG_68K_D_SCRATCH_1);
    // scratch = 0x80 if Z was set
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80);
    // clear Z bit in D7
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x7f);
    // D7 |= new Z bit
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);
}

// Set Z and C flags in D7 based on current 68k CCR, also set N=1 (for sub/cp)
void compile_set_znc_flags(struct code_block *block)
{
    // extract both flags first using scc (scc doesn't affect CCR)
    // seq D1 (D1 = 0xff if Z, 0x00 if NZ)
    emit_scc(block, 0x7, REG_68K_D_SCRATCH_1);
    // scs D7 (D7 = 0xff if C, 0x00 if NC)
    emit_scc(block, 0x5, REG_68K_D_FLAGS);

    // now CCR doesn't matter anymore
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80); // D1 = 0x80 if Z was set
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10); // D7 = 0x10 if C was set
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);   // D7 = Z_bit | C_bit

    // Add N=1 flag for subtract operations
    emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // D7 |= 0x40 (N flag)
}
