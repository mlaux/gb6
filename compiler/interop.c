#include "compiler.h"
#include "emitters.h"
#include "interop.h"

#define DMG_READ_PAGE_OFFSET 0x80
#define DMG_WRITE_PAGE_OFFSET (0x80 + (0x100 * 4))

// Retro68 uses D0-D2 as scratch so I have to push cycle count before calling
// back into C. i'm not sure if this is a mac calling convention or specific
// to this gcc port

// addr in D1, val_reg specifies value register
void compile_slow_dmg_write(struct code_block *block, uint8_t val_reg)
{
    // store current cycle count for lazy register evaluation, right now
    // it's just DIV but want to add more like lcd
    emit_move_l_dn_disp_an(block, REG_68K_D_CYCLE_COUNT, JIT_CTX_READ_CYCLES, REG_68K_A_CTX);
    // and push so retro68 doesn't erase
    emit_push_l_dn(block, REG_68K_D_CYCLE_COUNT); // 2
    emit_push_b_dn(block, val_reg); // 2
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1); // 2
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX); // 4
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1); // 4
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1); // 2
    emit_addq_l_an(block, 7, 8); // 2
    emit_pop_l_dn(block, REG_68K_D_CYCLE_COUNT); // 2
}

// inline dmg_write with fast paths - addr in D1, value in val_reg
static void compile_inline_dmg_write(struct code_block *block, uint8_t val_reg)
{
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
    compile_slow_dmg_write(block, val_reg);
    // falls through to done (offset 82)
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D4 (A register)
void compile_call_dmg_write_a(struct code_block *block)
{
    compile_inline_dmg_write(block, REG_68K_D_A);
    // compile_slow_dmg_write(block, REG_68K_D_A);
}

// Call dmg_write(dmg, addr, val) - addr in D1, val is immediate
void compile_call_dmg_write_imm(struct code_block *block, uint8_t val)
{
    emit_move_b_dn(block, 3, val);
    compile_inline_dmg_write(block, 3);
    // emit_move_b_dn(block, 0, val);
    // compile_slow_dmg_write(block, 0);
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D0
void compile_call_dmg_write_d0(struct code_block *block)
{
    // uses d0 as scratch so need to move to d3, but it's so long that
    // what's one more instruction...
    emit_move_b_dn_dn(block, 0, 3);
    compile_inline_dmg_write(block, 3);
    // compile_slow_dmg_write(block, 0);
}

// Emit slow path call to dmg_read - expects address in D1, returns in D0
void compile_slow_dmg_read(struct code_block *block)
{
    // store current cycle count for lazy DIV evaluation
    emit_move_l_dn_disp_an(block, REG_68K_D_CYCLE_COUNT, JIT_CTX_READ_CYCLES, REG_68K_A_CTX); // 4
    emit_push_l_dn(block, REG_68K_D_CYCLE_COUNT); // 2
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1); // 2
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX); // 4
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1); // 4
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1); // 2
    emit_addq_l_an(block, 7, 6); // 2
    emit_pop_l_dn(block, REG_68K_D_CYCLE_COUNT); // 2
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
    compile_slow_dmg_read(block);
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

// Slow path for dmg_read16 - addr in D1.w, result in D0.w
static void compile_slow_dmg_read16(struct code_block *block)
{
    emit_move_l_dn_disp_an(block, REG_68K_D_CYCLE_COUNT, JIT_CTX_READ_CYCLES, REG_68K_A_CTX);
    emit_push_l_dn(block, REG_68K_D_CYCLE_COUNT);
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_READ16, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 6);
    emit_pop_l_dn(block, REG_68K_D_CYCLE_COUNT);
}

// Call dmg_read16(dmg, addr) - addr in D1.w, result in D0.w
// Inline fast path for page table hits when both bytes on same page
void compile_call_dmg_read16(struct code_block *block)
{
    // Check if both bytes on same page (addr & 0xff != 0xff)
    // If low byte is 0xff, second byte would cross to next page
    // move.w d1, d0                     ; 2 bytes [0-1]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // andi.w #$00ff, d0                 ; 4 bytes [2-5]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x00ff);
    // cmpi.w #$00ff, d0                 ; 4 bytes [6-9]
    emit_cmpi_w_imm_dn(block, 0x00ff, REG_68K_D_SCRATCH_0);
    // beq.b slow_path (+46)             ; 2 bytes [10-11] -> offset 58
    emit_beq_b(block, 46);

    // Page table lookup
    // movea.l JIT_CTX_DMG(a4), a1       ; 4 bytes [12-15]
    emit_movea_l_disp_an_an(block, JIT_CTX_DMG, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // lea DMG_READ_PAGE_OFFSET(a1), a1  ; 4 bytes [16-19]
    emit_lea_disp_an_an(block, DMG_READ_PAGE_OFFSET, REG_68K_A_SCRATCH_1, REG_68K_A_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes [20-21]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // lsr.w #8, d0                      ; 2 bytes [22-23]
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_SCRATCH_0);
    // lsl.w #2, d0                      ; 2 bytes [24-25]
    emit_lsl_w_imm_dn(block, 2, REG_68K_D_SCRATCH_0);
    // movea.l (a1,d0.w), a1             ; 4 bytes [26-29]
    emit_movea_l_idx_an_an(block, 0, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_A_SCRATCH_1);
    // cmpa.w #0, a1                     ; 4 bytes [30-33]
    emit_cmpa_w_imm_an(block, 0, REG_68K_A_SCRATCH_1);
    // beq.b slow_path (+22)             ; 2 bytes [34-35] -> offset 58
    emit_beq_b(block, 22);

    // Fast read from page - read low byte, then high byte, combine
    // move.w d1, d0                     ; 2 bytes [36-37]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // andi.w #$ff, d0                   ; 4 bytes [38-41]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x00ff);
    // move.b (a1,d0.w), d3              ; 4 bytes [42-45] - low byte -> d3
    emit_move_b_idx_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_D_NEXT_PC);
    // addq.w #1, d0                     ; 2 bytes [46-47]
    emit_addq_w_dn(block, REG_68K_D_SCRATCH_0, 1);
    // move.b (a1,d0.w), d0              ; 4 bytes [48-51] - high byte -> d0.b
    emit_move_b_idx_an_dn(block, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_D_SCRATCH_0);
    // lsl.w #8, d0                      ; 2 bytes [52-53] - shift high byte up
    emit_lsl_w_imm_dn(block, 8, REG_68K_D_SCRATCH_0);
    // move.b d3, d0                     ; 2 bytes [54-55] - combine low byte
    emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_0);
    // bra.b done (+22)                  ; 2 bytes [56-57] -> offset 80
    emit_bra_b(block, 22);

    // slow_path: (offset 58)
    compile_slow_dmg_read16(block);
    // falls through to done (offset 80)
}

// Slow path for dmg_write16 - addr in D1.w, data in D0.w
static void compile_slow_dmg_write16(struct code_block *block)
{
    emit_move_l_dn_disp_an(block, REG_68K_D_CYCLE_COUNT, JIT_CTX_READ_CYCLES, REG_68K_A_CTX);
    emit_push_l_dn(block, REG_68K_D_CYCLE_COUNT);
    emit_push_w_dn(block, REG_68K_D_SCRATCH_0);
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE16, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);
    emit_addq_l_an(block, 7, 8);
    emit_pop_l_dn(block, REG_68K_D_CYCLE_COUNT);
}

// Call dmg_write16(dmg, addr, data) - addr in D1.w, data in D0.w
// Inline fast path for page table hits when both bytes on same page
void compile_call_dmg_write16_d0(struct code_block *block)
{
    // Save data to D3 before we use D0 as scratch
    // move.w d0, d3                     ; 2 bytes [0-1]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_0, REG_68K_D_NEXT_PC);

    // Check if both bytes on same page (addr & 0xff != 0xff)
    // move.w d1, d0                     ; 2 bytes [2-3]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // andi.w #$00ff, d0                 ; 4 bytes [4-7]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x00ff);
    // cmpi.w #$00ff, d0                 ; 4 bytes [8-11]
    emit_cmpi_w_imm_dn(block, 0x00ff, REG_68K_D_SCRATCH_0);
    // beq.b slow_path (+44)             ; 2 bytes [12-13] -> offset 58
    emit_beq_b(block, 44);

    // Page table lookup
    // movea.l JIT_CTX_DMG(a4), a1       ; 4 bytes [14-17]
    emit_movea_l_disp_an_an(block, JIT_CTX_DMG, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);
    // lea DMG_WRITE_PAGE_OFFSET(a1), a1 ; 4 bytes [18-21]
    emit_lea_disp_an_an(block, DMG_WRITE_PAGE_OFFSET, REG_68K_A_SCRATCH_1, REG_68K_A_SCRATCH_1);
    // move.w d1, d0                     ; 2 bytes [22-23]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // lsr.w #8, d0                      ; 2 bytes [24-25]
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_SCRATCH_0);
    // lsl.w #2, d0                      ; 2 bytes [26-27]
    emit_lsl_w_imm_dn(block, 2, REG_68K_D_SCRATCH_0);
    // movea.l (a1,d0.w), a1             ; 4 bytes [28-31]
    emit_movea_l_idx_an_an(block, 0, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0, REG_68K_A_SCRATCH_1);
    // cmpa.w #0, a1                     ; 4 bytes [32-35]
    emit_cmpa_w_imm_an(block, 0, REG_68K_A_SCRATCH_1);
    // beq.b slow_path (+20)             ; 2 bytes [36-37] -> offset 58
    emit_beq_b(block, 20);

    // Fast write to page - write low byte, then high byte
    // move.w d1, d0                     ; 2 bytes [38-39]
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // andi.w #$ff, d0                   ; 4 bytes [40-43]
    emit_andi_w_dn(block, REG_68K_D_SCRATCH_0, 0x00ff);
    // move.b d3, (a1,d0.w)              ; 4 bytes [44-47] - write low byte
    emit_move_b_dn_idx_an(block, REG_68K_D_NEXT_PC, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // lsr.w #8, d3                      ; 2 bytes [48-49] - shift high byte down
    emit_lsr_w_imm_dn(block, 8, REG_68K_D_NEXT_PC);
    // addq.w #1, d0                     ; 2 bytes [50-51]
    emit_addq_w_dn(block, REG_68K_D_SCRATCH_0, 1);
    // move.b d3, (a1,d0.w)              ; 4 bytes [52-55] - write high byte
    emit_move_b_dn_idx_an(block, REG_68K_D_NEXT_PC, REG_68K_A_SCRATCH_1, REG_68K_D_SCRATCH_0);
    // bra.b done (+26)                  ; 2 bytes [56-57] -> offset 84
    emit_bra_b(block, 26);

    // slow_path: (offset 58)
    // Restore data from D3 to D0 for slow path
    // move.w d3, d0                     ; 2 bytes [58-59]
    emit_move_w_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_SCRATCH_0);
    compile_slow_dmg_write16(block);
    // falls through to done (offset 84)
}
