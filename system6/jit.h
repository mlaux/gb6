#ifndef _JIT_H
#define _JIT_H

#include "types.h"
#include "dmg.h"

#define HALT_SENTINEL 0xffffffff

// runtime context for JIT, must match JIT_CTX offsets in compiler.h
typedef struct {
    /*  0 */ void *dmg;
    /*  4 */ void *read_func;
    /*  8 */ void *write_func;
    /*  c */ void *ei_di_func;
    /* 10 */ volatile u8 interrupt_check; // no longer used, can go away
    /* 11 */ volatile u8 current_rom_bank;
    /* 12 */ u8 _pad[2];
    /* 14 */ void **bank0_cache;
    /* 18 */ void ***banked_cache;
    /* 1c */ void **upper_cache;
    /* 20 */ void *dispatcher_return;
    /* 24 */ void *read16_func;
    /* 28 */ void *write16_func;
    /* 2c */ u32 cycles_accumulated;  // GB cycles accumulated by compiled code
    /* 30 */ void *patch_helper;  // patch_helper routine for lazy block patching
    /* 34 */ u32 read_cycles; // in-flight cycles at time of dmg_read call
    /* 38 */ void *wram_base; // dmg->main_ram for inline high RAM access
    /* 3c */ u32 *frame_cycles_ptr; // pointer to dmg->frame_cycles for HALT
    /* 40 */ u32 temp1;
    /* 44 */ u32 temp2;
    /* 48 */ u16 gb_sp;       // GB stack pointer value (always valid)
    /* 4a */ u16 _pad2;
    /* 4c */ long sp_adjust;   // non-zero if A3 points to WRAM (0xC000 - wram_base)
} jit_context;

extern jit_context jit_ctx;
extern int jit_halted;
extern int dmg_reads, dmg_writes;

void jit_init(struct dmg *dmg);

int jit_step(struct dmg *dmg);

void jit_cleanup(void);

#endif