#include <Memory.h>
#include <Timer.h>

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "jit.h"
#include "compiler.h"
#include "cpu.h"
#include "dmg.h"
#include "lru.h"
#include "dispatcher_asm.h"
#include "emulator.h"
#include "debug.h"

#define CYCLES_PER_INTERRUPT 70224

// register state that persists between block executions
struct {
  u32 d4, d5, d6, d7;
  u32 a2, a3, a4;
} jit_regs;
u32 jit_d0;

// exposed to main emulator.c
jit_context jit_ctx;
int jit_halted = 0;

// Compile-time context for address calculation
static struct compile_ctx compile_ctx;

static long last_wall_ticks;

// this is a huge context switch and my main goal is to do this as little as
// possible. currently it will not return to C when jumping to another compiled 
// block. it still does to check and handle interrupts, though. 
static void execute_block(void *code)
{
  asm volatile(
    // save callee-saved registers
    "movem.l %%d2-%%d7/%%a2-%%a4, -(%%sp)\n\t"

    // copy code pointer to A0
    "movea.l %[code], %%a0\n\t"

    // load GB state into 68k registers
    "lea %[jit_regs], %%a1\n\t"
    "movem.l (%%a1), %%d4-%%d7/%%a2-%%a4\n\t"

    // call the generated code, this can then chain to other blocks
    "jsr (%%a0)\n\t"

    // save results back to memory
    "move.l %%d0, %[out_d0]\n\t"
    "lea %[jit_regs], %%a0\n\t"
    "movem.l %%d4-%%d7/%%a2-%%a3, (%%a0)\n\t"

    // restore callee-saved registers
    "movem.l (%%sp)+, %%d2-%%d7/%%a2-%%a4\n\t"

    : [out_d0] "=m" (jit_d0)
    : [jit_regs] "m" (jit_regs),
      [code] "a" (code)
    : "d0", "d1", "a0", "a1", "cc", "memory"
  );
}

// Initialize JIT state for a new emulation session
void jit_init(struct dmg *dmg)
{
    int k;

    // can just remove this...
    compiler_init();

    // also clears old blocks
    lru_init();

    memset(&jit_regs, 0, sizeof jit_regs);

    // Initialize compile-time context
    compile_ctx.dmg = dmg;
    compile_ctx.read = dmg_read;
    compile_ctx.wram_base = dmg->main_ram;
    compile_ctx.hram_base = dmg->zero_page;

    jit_ctx.dmg = dmg;
    jit_ctx.read_func = dmg_read;
    jit_ctx.write_func = dmg_write;
    jit_ctx.ei_di_func = dmg_ei_di;
    jit_ctx.interrupt_check = 0;
    jit_ctx.current_rom_bank = 1;  // bank 1 is default after boot
    jit_ctx.bank0_cache = bank0_cache;
    jit_ctx.banked_cache = banked_cache;
    jit_ctx.upper_cache = upper_cache;
    jit_ctx.dispatcher_return = (void *) dispatcher_code;

    // Initialize SP to point to top of HRAM (0xFFFE)
    // A3 is the native pointer, gb_sp is the GB address, sp_adjust converts between them
    jit_regs.a3 = (unsigned long) (dmg->zero_page + 0xfffe - 0xff80);
    jit_ctx.gb_sp = 0xfffe;
    jit_ctx.sp_adjust = 0xff80 - (u32) dmg->zero_page;
    jit_regs.a4 = (unsigned long) &jit_ctx;

    jit_halted = 0;
    last_wall_ticks = TickCount();
}

int jit_step(struct dmg *dmg)
{
    struct code_block *block;
    char buf[64];
    int took_interrupt;
    u32 finished_ticks;
    u32 wall_ticks;
    u16 start_pc = dmg->cpu->pc;

    if (jit_halted) {
        return 0;
    }

    // clear interrupt check before executing so we can make progress
    // even if the timer fired during ProcessEvents
    jit_ctx.interrupt_check = 0;

    // Look up or compile block
    block = cache_lookup(start_pc, jit_ctx.current_rom_bank);

    if (!block) {
        u32 free_heap;
        lru_ensure_memory();
        free_heap = (u32) FreeMem();
        sprintf(buf, "Compiling $%02x:%04x free=%u",
                jit_ctx.current_rom_bank, start_pc, free_heap);
        set_status_bar(buf);
        block = compile_block(start_pc, &compile_ctx);
        if (!block) {
            u32 unused;

            // try to free memory by evicting LRU blocks
            lru_clear_all();
            MaxMem(&unused);
            block = compile_block(start_pc, &compile_ctx);

            if (!block) {
              sprintf(buf, "JIT: alloc fail pc=%04x", start_pc);
              set_status_bar(buf);
              jit_halted = 1;
              return 0;
            }
        }

        if (block->error) {
            sprintf(buf, "Error pc=%04x op=%02x", block->failed_address, block->failed_opcode);
            set_status_bar(buf);
            jit_halted = 1;
            block_free(block);
            return 0;
        }

        //debug_log_block(block);
        lru_add_block(block, start_pc, jit_ctx.current_rom_bank);
    } else {
        if (block->lru_node) {
            lru_promote((lru_node *) block->lru_node);
        }
        set_status_bar("Running");
    }

    execute_block(block->code);

    // Get next PC from D0
    if (jit_d0 == HALT_SENTINEL) {
        set_status_bar("HALT");
        jit_halted = 1;
        return 0;
    }

    took_interrupt = 0;
    if (dmg->cpu->interrupt_enable) {
      u8 pending = dmg->interrupt_enable_mask & dmg->interrupt_request_mask & 0x1f;
      if (pending) {
        static const u16 handlers[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
        int k;
        for (k = 0; k < 5; k++) {
          if (pending & (1 << k)) {
            // clear IF bit and disable IME
            dmg->interrupt_request_mask &= ~(1 << k);
            dmg->cpu->interrupt_enable = 0;

            // push PC to stack
            u8 *sp_ptr = (u8 *) jit_regs.a3;
            sp_ptr -= 2;
            sp_ptr[1] = (jit_d0 >> 8) & 0xff;
            sp_ptr[0] = jit_d0 & 0xff;
            jit_regs.a3 = (unsigned long) sp_ptr;
            jit_ctx.gb_sp -= 2;

            // Jump to handler
            jit_d0 = handlers[k];
            took_interrupt = 1;
            break;
          }
        }
      }
    }

    dmg->cpu->pc = (u16) jit_d0;

    finished_ticks = TickCount();
    wall_ticks = finished_ticks - last_wall_ticks;
    last_wall_ticks = finished_ticks;

    // don't sync lcd if interrupt just happened to prevent vblank spam
    // where the "main thread" can't make progress
    if (!took_interrupt) {
      dmg_sync_hw(dmg, wall_ticks * CYCLES_PER_INTERRUPT);
    }

    return 1;
}