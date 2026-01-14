#include "compiler.h"
#include "emitters.h"
#include "interop.h"

#define DMG_READ_PAGE_OFFSET 0x80
#define DMG_WRITE_PAGE_OFFSET (0x80 + (0x100 * 4))

// Value source for write operations
typedef enum {
    WRITE_VAL_A,    // value in D4 (A register)
    WRITE_VAL_D0,   // value in D0
    WRITE_VAL_IMM   // immediate value
} write_val_src;

// slow path call to dmg_write - addr in D1, val_reg specifies value register
void emit_slow_dmg_write(struct code_block *block, uint8_t val_reg)
{
    // store current cycle count for lazy register evaluation, right now
    // it's just DIV but want to add more like lcd
    emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_2, JIT_CTX_READ_CYCLES, REG_68K_A_CTX);
    emit_push_l_dn(block, REG_68K_D_SCRATCH_2); // 2
    emit_push_b_dn(block, val_reg); // 2
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1); // 2
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX); // 4
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1); // 4
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1); // 2
    emit_addq_l_an(block, 7, 8); // 2
    emit_pop_l_dn(block, REG_68K_D_SCRATCH_2); // 2
}

// inline dmg_write with fast paths - addr in D1, value in val_reg
// val_reg should be D4 for WRITE_VAL_A, or D0 with value preserved for WRITE_VAL_D0
static void compile_inline_dmg_write(struct code_block *block, uint8_t val_reg)
{
    // For write, page table offset is 1024 which is too large for 8-bit indexed addressing
    // So we use LEA to get the write_page base first

    // Fast path: check write page table
    // movea.l JIT_CTX_DMG(a4), a0       ; 4 bytes [0-3]
    emit_movea_l_disp_an_an(block, JIT_CTX_DMG, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // lea DMG_WRITE_PAGE_OFFSET(a0), a0 ; 4 bytes [4-7]
    emit_lea_disp_an_an(block, DMG_WRITE_PAGE_OFFSET, REG_68K_A_SCRATCH_1, REG_68K_A_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes [8-9]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // lsr.w #8, d0                      ; 2 bytes [10-11]
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_SCRATCH_0);
    // lsl.w #2, d0                      ; 2 bytes [12-13]
    emit_lsl_w_imm_dn(block, 2, REG_68K_D_SCRATCH_0);
    // movea.l (a0,d0.w), a0             ; 4 bytes [14-17]
    emit_movea_l_idx_an_an(block, 0, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_A_SCRATCH_1);
    // cmpa.w #0, a0                     ; 4 bytes [18-21]
    emit_cmpa_w_imm_an(block, 0, REG_68K_A_SCRATCH_1);
    // beq.s check_hram (+12)            ; 2 bytes [22-23] -> offset 36
    emit_beq_b(block, 12);

    // Page hit (offset 24):
    // move.w d1, d0                     ; 2 bytes
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // andi.w #$ff, d0                   ; 4 bytes
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x00ff);
    // move.b val_reg, (a0,d0.w)         ; 4 bytes
    emit_move_b_dn_idx_an(block, val_reg, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // bra.s done (+46)                  ; 2 bytes
    emit_bra_b(block, 46);

    // check_hram: (offset 36)
    // cmpi.w #$ff80, d1                 ; 4 bytes
    emit_cmpi_w_imm_dn(block, 0xff80, REG_68K_D_SCRATCH_1);
    // bcs.s slow_path (+16)             ; 2 bytes
    emit_bcs_b(block, 16);

    // hram access (offset 42):
    // movea.l JIT_CTX_HRAM_BASE(a4), a0 ; 4 bytes
    emit_movea_l_disp_an_an(block, JIT_CTX_HRAM_BASE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // subi.w #$ff80, d0                 ; 4 bytes
    emit_subi_w_dn(block, 0xff80, REG_68K_D_SCRATCH_0);
    // move.b val_reg, (a0,d0.w)         ; 4 bytes
    emit_move_b_dn_idx_an(block, val_reg, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // bra.s done (+24)                  ; 2 bytes
    emit_bra_b(block, 24);

    // slow_path: (offset 58)
    emit_slow_dmg_write(block, val_reg);
    // falls through to done (offset 82)
}

static void compile_dmg_write_internal(
    struct code_block *block,
    write_val_src src,
    uint8_t imm_val
) {
    uint8_t val_reg;

    switch (src) {
    case WRITE_VAL_A:
        // Value in D4, can use inline fast path directly
        compile_inline_dmg_write(block, REG_68K_D_A);
        return;
    case WRITE_VAL_D0:
        // Value in D0, but D0 is used as scratch in inline path
        // For now, use slow path to avoid complexity
        val_reg = REG_68K_D_SCRATCH_0;
        break;
    case WRITE_VAL_IMM:
        emit_move_b_dn(block, REG_68K_D_SCRATCH_0, imm_val);
        val_reg = REG_68K_D_SCRATCH_0;
        break;
    }

    emit_slow_dmg_write(block, val_reg);
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

// Emit slow path call to dmg_read - expects address in D1, returns in D0
void emit_slow_dmg_read(struct code_block *block)
{
    // store current cycle count for lazy DIV evaluation
    emit_move_l_dn_disp_an(block, REG_68K_D_SCRATCH_2, JIT_CTX_READ_CYCLES, REG_68K_A_CTX);
    emit_push_l_dn(block, REG_68K_D_SCRATCH_2); // 2
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);
    emit_pop_l_dn(block, REG_68K_D_SCRATCH_2); // 2
}

// Call dmg_read(dmg, addr) - addr in D1, result stays in D0
// Generates inline fast paths for page table hits and high RAM
void compile_call_dmg_read(struct code_block *block)
{
    // Fast path 1: check page table
    // movea.l JIT_CTX_DMG(a4), a0       ; 4 bytes [0-3]
    emit_movea_l_disp_an_an(block, JIT_CTX_DMG, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // lea DMG_READ_PAGE_OFFSET(a0), a0  ; 4 bytes [4-7]
    emit_lea_disp_an_an(block, DMG_READ_PAGE_OFFSET, REG_68K_A_SCRATCH_1, REG_68K_A_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes [8-9]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // lsr.w #8, d0                      ; 2 bytes [10-11]
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_SCRATCH_0);
    // lsl.w #2, d0                      ; 2 bytes [12-13]
    emit_lsl_w_imm_dn(block, 2, REG_68K_D_SCRATCH_0);
    // movea.l (a0,d0.w), a0             ; 4 bytes [14-17]
    emit_movea_l_idx_an_an(block, 0, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_A_SCRATCH_1);
    // cmpa.w #0, a0                     ; 4 bytes [18-21]
    emit_cmpa_w_imm_an(block, 0, REG_68K_A_SCRATCH_1);
    // beq.s check_hram (+12)            ; 2 bytes [22-23] -> offset 36
    emit_beq_b(block, 12);

    // Page hit (offset 24):
    // move.w d1, d0                     ; 2 bytes [24-25]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // andi.w #$ff, d0                   ; 4 bytes [26-29]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x00ff);
    // move.b (a0,d0.w), d0              ; 4 bytes [30-33]
    emit_move_b_idx_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_0);
    // bra.s done (+44)                  ; 2 bytes [34-35] -> offset 80
    emit_bra_b(block, 44);

    // check_hram: (offset 36)
    // cmpi.w #$ff80, d1                 ; 4 bytes [36-39]
    emit_cmpi_w_imm_dn(block, 0xff80, REG_68K_D_SCRATCH_1);
    // bcs.s slow_path (+16)             ; 2 bytes [40-41] -> offset 58
    emit_bcs_b(block, 16);

    // hram access (offset 42):
    // HRAM is now offset 0 in dmg, so this could be simplified a bit i think
    // movea.l JIT_CTX_HRAM_BASE(a4), a0 ; 4 bytes [42-45]
    emit_movea_l_disp_an_an(block, JIT_CTX_HRAM_BASE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes [46-47]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // subi.w #$ff80, d0                 ; 4 bytes [48-51]
    emit_subi_w_dn(block, 0xff80, REG_68K_D_SCRATCH_0);
    // move.b (a0,d0.w), d0              ; 4 bytes [52-55]
    emit_move_b_idx_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_0);
    // bra.s done (+22)                  ; 2 bytes [56-57] -> offset 80
    emit_bra_b(block, 22);

    // slow_path: (offset 58)
    emit_slow_dmg_read(block);
    // falls through to done (offset 80)
}

// Call dmg_read(dmg, addr) - addr in D1, result goes to D4 (A register)
void compile_call_dmg_read_a(struct code_block *block)
{
    compile_call_dmg_read(block);
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
