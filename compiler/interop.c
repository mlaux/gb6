#include "compiler.h"
#include "emitters.h"
#include "interop.h"

// Offset of page arrays in struct dmg
#define DMG_READ_PAGE_OFFSET 12
#define DMG_WRITE_PAGE_OFFSET (12 + 256 * 4)  // after read_page[256]

// Value source for write operations
typedef enum {
    WRITE_VAL_A,    // value in D4 (A register)
    WRITE_VAL_D0,   // value in D0
    WRITE_VAL_IMM   // immediate value
} write_val_src;

// Internal helper for dmg_write with page table fast path
// addr in D1, value source specified by src parameter
static void compile_dmg_write_internal(
    struct code_block *block,
    write_val_src src,
    uint8_t imm_val
) {
    // Fast path sizes for branch calculation:
    // move.w d1,d0 (2) + andi.w (4) + move.b indexed (4) + bra.b (2) = 12 bytes
    // Slow path size:
    // push.b (2) + push.w (2) + push.l (4) + movea.l (4) + jsr (2) + addq.l (2) = 16 bytes

    uint8_t val_reg;

    switch (src) {
    case WRITE_VAL_A:
        val_reg = REG_68K_D_A; // use D4 directly
        break;
    case WRITE_VAL_D0:
        emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_2);  // save D0 to D2
        val_reg = REG_68K_D_SCRATCH_2;
        break;
    case WRITE_VAL_IMM:
        emit_move_b_dn(block, REG_68K_D_SCRATCH_2, imm_val);  // load imm to D2
        val_reg = REG_68K_D_SCRATCH_2;
        break;
    }

    emit_push_b_dn(block, val_reg); // push value
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1); // push address
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX); // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 8); // clean up stack
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D4 (A register)
void compile_call_dmg_write(struct code_block *block)
{
    compile_dmg_write_internal(block, WRITE_VAL_A, 0);
}

// Call dmg_write(dmg, addr, val) - addr in D1, val is immediate
void compile_call_dmg_write_imm(struct code_block *block, uint8_t val)
{
    compile_dmg_write_internal(block, WRITE_VAL_IMM, val);
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D0
void compile_call_dmg_write_d0(struct code_block *block)
{
    compile_dmg_write_internal(block, WRITE_VAL_D0, 0);
}

// Call dmg_read(dmg, addr) - addr in D1, result stays in D0
void compile_call_dmg_read_to_d0(struct code_block *block)
{
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);
}

// Call dmg_read(dmg, addr) - addr in D1, result goes to D4 (A register)
void compile_call_dmg_read(struct code_block *block)
{
    compile_call_dmg_read_to_d0(block);
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

// Slow pop: read word from [gb_sp], increment gb_sp by 2
// Result in D1.w (low byte at [gb_sp], high byte at [gb_sp+1])
void compile_slow_pop_to_d1(struct code_block *block)
{
    // Load gb_sp into D1
    emit_move_w_disp_an_dn(block, JIT_CTX_GB_SP, REG_68K_A_CTX, REG_68K_D_SCRATCH_1);

    // Read low byte: dmg_read(dmg, gb_sp)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);
    // D0 = low byte, save to D2
    emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_2);

    // Read high byte: dmg_read(dmg, gb_sp+1)
    emit_move_w_disp_an_dn(block, JIT_CTX_GB_SP, REG_68K_A_CTX, REG_68K_D_SCRATCH_1);
    emit_addq_w_dn(block, REG_68K_D_SCRATCH_1, 1);
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);
    // D0 = high byte

    // Combine: D1.w = (high << 8) | low
    emit_rol_w_8(block, REG_68K_D_NEXT_PC);  // D0.w = high byte in upper
    emit_move_b_dn_dn(block, REG_68K_D_SCRATCH_2, REG_68K_D_NEXT_PC);  // D0.b = low
    emit_move_w_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_1);  // D1.w = result

    // Increment gb_sp by 2
    emit_addi_w_disp_an(block, 2, JIT_CTX_GB_SP, REG_68K_A_CTX);
}
