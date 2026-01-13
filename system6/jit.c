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
#include "lcd.h"
#include "dispatcher_asm.h"
#include "emulator.h"
#include "debug.h"

static u32 time_in_jit = 0;
static u32 time_in_sync = 0;
static u32 time_in_lookup = 0;
static u32 call_count = 0;
static u32 last_report_tick = 0;

int dmg_reads, dmg_writes;

// register state that persists between block executions
struct {
  u32 d2; // accumulated cycles, output
  u32 d3; // next pc, output only
  u32 d4, d5, d6, d7; // a, bc, de, f
  u32 a2, a3, a4; // hl, sp, ctx
} jit_regs;

// exposed to main emulator.c
jit_context jit_ctx;
int jit_halted = 0;

// compile-time context for address calculation
static struct compile_ctx compile_ctx;

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
    "movem.l (%%a1), %%d2-%%d7/%%a2-%%a4\n\t"

    // call the generated code, this can then chain to other blocks
    "jsr (%%a0)\n\t"

    // save results back to memory
    "lea %[jit_regs], %%a0\n\t"
    "movem.l %%d2-%%d7/%%a2-%%a3, (%%a0)\n\t"

    // restore callee-saved registers
    "movem.l (%%sp)+, %%d2-%%d7/%%a2-%%a4\n\t"

    : // no outputs
    : [jit_regs] "m" (jit_regs),
      [code] "a" (code)
    : "d0", "d1", "a0", "a1", "cc", "memory"
  );
}

// Initialize JIT state for a new emulation session
void jit_init(struct dmg *dmg)
{
    compiler_init();

    // Clear any existing cache from previous session
    cache_clear_all();
    cache_free_bank_arrays();

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
    jit_ctx.current_rom_bank = 1;  // bank 1 is default after boot
    jit_ctx.bank0_cache = bank0_cache;
    jit_ctx.banked_cache = banked_cache;
    jit_ctx.upper_cache = upper_cache;
    jit_ctx.dispatcher_return = (void *) get_dispatcher_code();
    jit_ctx.patch_helper = (void *) get_patch_helper_code();
    jit_ctx.hram_base = dmg->zero_page;

    jit_regs.d2 = 0;
    jit_regs.d3 = 0x100;
    // Initialize SP to point to top of HRAM (0xFFFE)
    // A3 is the native pointer, gb_sp is the GB address, sp_adjust converts between them
    jit_regs.a3 = (unsigned long) (dmg->zero_page + 0xfffe - 0xff80);
    jit_ctx.gb_sp = 0xfffe;
    jit_ctx.sp_adjust = 0xff80 - (u32) dmg->zero_page;
    jit_regs.a4 = (unsigned long) &jit_ctx;

    jit_halted = 0;
}

static u32 cycles_min = 0xffffffff, cycles_max;

int jit_step(struct dmg *dmg)
{
    struct code_block *block;
    char buf[64];
    u32 t0, t1, t2, t3, cycles;
    t0 = TickCount();

    if (jit_halted) {
        return 0;
    }

    // Look up or compile block
    block = cache_lookup(jit_regs.d3, jit_ctx.current_rom_bank);

    if (!block) {
        u32 free_heap;
        cache_ensure_memory();
        free_heap = (u32) FreeMem();
        sprintf(buf, "Compiling $%02x:%04x free=%u",
                jit_ctx.current_rom_bank, jit_regs.d3, free_heap);
        set_status_bar(buf);
        block = compile_block(jit_regs.d3, &compile_ctx);
        if (!block) {
            sprintf(buf, "JIT: alloc fail pc=%04x", jit_regs.d3);
            set_status_bar(buf);
            jit_halted = 1;
            return 0;
        }

        if (block->error) {
            sprintf(buf, "Error pc=%04x op=%02x", block->failed_address, block->failed_opcode);
            set_status_bar(buf);
            jit_halted = 1;
            block_free(block);
            return 0;
        }

        cache_store(jit_regs.d3, jit_ctx.current_rom_bank, block);
    }

    t1 = TickCount();
    execute_block(block->code);
    t2 = TickCount();

    // Get next PC from D3
    if (jit_regs.d3 == HALT_SENTINEL) {
        set_status_bar("HALT");
        jit_halted = 1;
        return 0;
    }

    // sync hardware with cycles accumulated by compiled code
    dmg_sync_hw(dmg, jit_regs.d2);
    cycles = jit_regs.d2;
    if (cycles < cycles_min) {
      cycles_min = cycles;
    }
    if (cycles > cycles_max) {
      cycles_max = cycles;
    }
    jit_regs.d2 = 0;

    if (dmg->interrupt_enable) {
      u8 pending = dmg->zero_page[0x7f] & dmg->interrupt_request_mask & 0x1f;
      if (pending) {
        static const u16 handlers[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
        int k;
        for (k = 0; k < 5; k++) {
          if (pending & (1 << k)) {
            // clear IF bit and disable IME
            dmg->interrupt_request_mask &= ~(1 << k);
            dmg->interrupt_enable = 0;

            // push PC to stack
            u8 *sp_ptr = (u8 *) jit_regs.a3;
            sp_ptr -= 2;
            sp_ptr[1] = (jit_regs.d3 >> 8) & 0xff;
            sp_ptr[0] = jit_regs.d3 & 0xff;
            jit_regs.a3 = (unsigned long) sp_ptr;
            jit_ctx.gb_sp -= 2;

            // Jump to handler
            jit_regs.d3 = handlers[k];
            break;
          }
        }
      }
    }

    t3 = TickCount();
    time_in_lookup += t1 - t0;
    time_in_jit += t2 - t1;
    time_in_sync += t3 - t2;
    call_count++;
    if (call_count % 100 == 0) {
      static u32 last_lookup = 0, last_jit = 0, last_sync = 0;
      u32 now = TickCount();
      u32 elapsed = now - last_report_tick;
      u32 exits_per_sec = elapsed > 0 ? (100 * 60) / elapsed : 0;

      u32 d_lookup = time_in_lookup - last_lookup;
      u32 d_jit = time_in_jit - last_jit;
      u32 d_sync = time_in_sync - last_sync;

      u32 pct_lookup = elapsed > 0 ? (d_lookup * 100) / elapsed : 0;
      u32 pct_jit = elapsed > 0 ? (d_jit * 100) / elapsed : 0;
      u32 pct_sync = elapsed > 0 ? (d_sync * 100) / elapsed : 0;

      last_lookup = time_in_lookup;
      last_jit = time_in_jit;
      last_sync = time_in_sync;
      last_report_tick = now;

      sprintf(buf, "E/s:%lu J:%lu%% S:%lu%%", exits_per_sec, pct_jit, pct_sync);
      set_status_bar(buf);
    }

    return 1;
}