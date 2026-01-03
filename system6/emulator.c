/* Game Boy emulator for 68k Macs
   emulator.c - entry point */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <Quickdraw.h>
#include <StandardFile.h>
#include <Dialogs.h>
#include <Menus.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <Memory.h>
#include <Sound.h>
#include <Timer.h>
#include <Files.h>
#include <SegLoad.h>
#include <Resources.h>

#include "emulator.h"

#include "cpu.h"
#include "dmg.h"
#include "lcd.h"
#include "rom.h"
#include "mbc.h"

#include "dialogs.h"
#include "input.h"
#include "lcd_mac.h"
#include "dispatcher_asm.h"

#include "compiler.h"

// -- JIT INFRASTRUCTURE --

#define BANK0_CACHE_SIZE 0x4000
#define BANKED_CACHE_SIZE 0x4000
#define UPPER_CACHE_SIZE 0x8000
#define MAX_ROM_BANKS 256
#define HALT_SENTINEL 0xffffffff

// Block caches for different memory regions
// Bank 0: 0x0000-0x3FFF (always mapped)
static struct code_block *bank0_cache[BANK0_CACHE_SIZE];
// Switchable banks: 0x4000-0x7FFF (allocated on demand per bank)
static struct code_block **banked_cache[MAX_ROM_BANKS];
// Upper region: 0x8000-0xFFFF (WRAM, HRAM, etc.)
static struct code_block *upper_cache[UPPER_CACHE_SIZE];

// -- LRU CACHE MANAGEMENT --

// 4096 blocks * sizeof(lru_node) = 64k
#define MAX_CACHED_BLOCKS 4096

typedef struct lru_node {
    struct code_block *block;
    u16 pc;
    u8 bank;
    u8 in_use;
    struct lru_node *next;
    struct lru_node *prev;
} lru_node;

static lru_node lru_pool[MAX_CACHED_BLOCKS];
static lru_node *lru_head;  // most recently used
static lru_node *lru_tail;  // least recently used
static lru_node *lru_free;  // free list head
static int lru_count;

// Debug logging - open/close each time to avoid losing data on crash
static int debug_enabled = 1;

static long last_wall_ticks;

void debug_log_string(const char *str)
{
  short fref;
  char buf[128];
  long len;
  OSErr err;
  err = FSOpen("\pjit_log.txt", 0, &fref);
  if (err == fnfErr) {
      Create("\pjit_log.txt", 0, 'ttxt', 'TEXT');
      err = FSOpen("\pjit_log.txt", 0, &fref);
  }
  if (err != noErr) return;
  // Seek to end
  SetFPos(fref, fsFromLEOF, 0);
  len = strlen(str);
  FSWrite(fref, &len, str);
  buf[0] = '\n';
  len = 1;
  FSWrite(fref, &len, buf);

  FSClose(fref);
}

static void debug_log_block(struct code_block *block)
{
    short fref;
    char buf[128];
    long len;
    int k;
    OSErr err;

    if (!debug_enabled) return;

    err = FSOpen("\pjit_log.txt", 0, &fref);
    if (err == fnfErr) {
        Create("\pjit_log.txt", 0, 'ttxt', 'TEXT');
        err = FSOpen("\pjit_log.txt", 0, &fref);
    }
    if (err != noErr) return;

    // Seek to end
    SetFPos(fref, fsFromLEOF, 0);

    // Write block header
    sprintf(buf, "Block %04x->%04x (%d bytes):\n",
            block->src_address, block->end_address, (int) block->length);
    len = strlen(buf);
    FSWrite(fref, &len, buf);

    // Write hex dump of generated code
    for (k = 0; k < block->length; k++) {
        sprintf(buf, "%02x", block->code[k]);
        len = 2;
        FSWrite(fref, &len, buf);
        if ((k & 15) == 15 || k == block->length - 1) {
            buf[0] = '\n';
            len = 1;
            FSWrite(fref, &len, buf);
        } else {
            buf[0] = ' ';
            len = 1;
            FSWrite(fref, &len, buf);
        }
    }

    // Newline separator
    buf[0] = '\n';
    len = 1;
    FSWrite(fref, &len, buf);

    FSClose(fref);
}

// register state that persists between block executions
static unsigned long jit_dregs[8];
static unsigned long jit_aregs[8];

// runtime context for JIT (must match JIT_CTX_* offsets)
// Offsets:
//   0: dmg
//   4: read_func
//   8: write_func
//  12: ei_di_func
//  16: interrupt_check (1 byte)
//  17: current_rom_bank (1 byte)
//  18: _pad (2 bytes)
//  20: bank0_cache
//  24: banked_cache
//  28: upper_cache
//  32: dispatcher_return
typedef struct {
    void *dmg;
    void *read_func;
    void *write_func;
    void *ei_di_func;
    volatile char interrupt_check;
    volatile u8 current_rom_bank;
    char _pad[2];
    struct code_block **bank0_cache;
    struct code_block ***banked_cache;
    struct code_block **upper_cache;
    void *dispatcher_return;
    int32_t sp_adjust;
} jit_context;

static jit_context jit_ctx;

// Compile-time context for address calculation
static struct compile_ctx compile_ctx;

// Helper: look up cached block for given PC and bank
static struct code_block *cache_lookup(u16 pc, u8 bank)
{
    if (pc < 0x4000) {
        return bank0_cache[pc];
    }
    if (pc < 0x8000) {
        if (!banked_cache[bank]) {
            return NULL;
        }
        return banked_cache[bank][pc - 0x4000];
    }
    return upper_cache[pc - 0x8000];
}

// Helper: store block in cache for given PC and bank
static void cache_store(u16 pc, u8 bank, struct code_block *block)
{
    if (pc < 0x4000) {
        bank0_cache[pc] = block;
    } else if (pc < 0x8000) {
        if (!banked_cache[bank]) {
            // Allocate cache for this bank on demand
            banked_cache[bank] = (struct code_block **)
                calloc(BANKED_CACHE_SIZE, sizeof(struct code_block *));
            if (!banked_cache[bank]) {
                return;  // allocation failed, block won't be cached
            }
        }
        banked_cache[bank][pc - 0x4000] = block;
    } else {
        upper_cache[pc - 0x8000] = block;
    }
}

// Helper: free all blocks in a cache array
static void cache_clear_array(struct code_block **cache, int size)
{
    int k;
    for (k = 0; k < size; k++) {
        if (cache[k]) {
            block_free(cache[k]);
            cache[k] = NULL;
        }
    }
}

// -- LRU FUNCTIONS --

// Initialize LRU system
static void lru_init(void)
{
    int k;

    lru_head = NULL;
    lru_tail = NULL;
    lru_count = 0;

    // Build free list
    lru_free = &lru_pool[0];
    for (k = 0; k < MAX_CACHED_BLOCKS - 1; k++) {
        lru_pool[k].next = &lru_pool[k + 1];
        lru_pool[k].in_use = 0;
        lru_pool[k].block = NULL;
    }
    lru_pool[MAX_CACHED_BLOCKS - 1].next = NULL;
    lru_pool[MAX_CACHED_BLOCKS - 1].in_use = 0;
    lru_pool[MAX_CACHED_BLOCKS - 1].block = NULL;
}

// Remove node from LRU list (but don't free it)
static void lru_unlink(lru_node *node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        lru_head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        lru_tail = node->prev;
    }
    node->prev = NULL;
    node->next = NULL;
}

// Add node to front of LRU list (most recent)
static void lru_push_front(lru_node *node)
{
    node->prev = NULL;
    node->next = lru_head;
    if (lru_head) {
        lru_head->prev = node;
    } else {
        lru_tail = node;
    }
    lru_head = node;
}

// Promote node to front (on cache hit)
static void lru_promote(lru_node *node)
{
    if (node == lru_head) {
        return;
    }
    lru_unlink(node);
    lru_push_front(node);
}

// Clear cache entry for a node
static void lru_clear_cache_entry(lru_node *node)
{
    u16 pc = node->pc;
    u8 bank = node->bank;

    if (pc < 0x4000) {
        bank0_cache[pc] = NULL;
    } else if (pc < 0x8000) {
        if (banked_cache[bank]) {
            banked_cache[bank][pc - 0x4000] = NULL;
        }
    } else {
        upper_cache[pc - 0x8000] = NULL;
    }
}

// Evict least recently used block
static void lru_evict_one(void)
{
    lru_node *victim;

    if (!lru_tail) {
        return;
    }

    victim = lru_tail;
    lru_unlink(victim);

    // Clear cache entry
    lru_clear_cache_entry(victim);

    // Free the block
    if (victim->block) {
        block_free(victim->block);
        victim->block = NULL;
    }

    // Return to free list
    victim->in_use = 0;
    victim->next = lru_free;
    lru_free = victim;
    lru_count--;
}

// Get a free node, evicting if necessary
static lru_node *lru_alloc_node(void)
{
    lru_node *node;

    if (!lru_free) {
        lru_evict_one();
    }

    if (!lru_free) {
        return NULL;  // shouldn't happen
    }

    node = lru_free;
    lru_free = node->next;
    node->next = NULL;
    node->prev = NULL;
    node->in_use = 1;
    node->block = NULL;
    lru_count++;

    return node;
}

// Add a new block to LRU and cache
static void lru_add_block(struct code_block *block, u16 pc, u8 bank)
{
    lru_node *node = lru_alloc_node();

    if (!node) {
        return;
    }

    node->block = block;
    node->pc = pc;
    node->bank = bank;

    // Store back-pointer for promotion on hit
    block->lru_node = node;

    lru_push_front(node);

    // Store in cache
    cache_store(pc, bank, block);
}

// Clear all LRU entries (for reset)
static void lru_clear_all(void)
{
    lru_node *node;
    int k;

    // Free all blocks in use
    for (k = 0; k < MAX_CACHED_BLOCKS; k++) {
        node = &lru_pool[k];
        if (node->in_use && node->block) {
            lru_clear_cache_entry(node);
            block_free(node->block);
            node->block = NULL;
        }
    }

    // Reinitialize
    lru_init();
}

// Proactively evict blocks when memory is low
#define MIN_FREE_HEAP 65536L

static void lru_ensure_memory(void)
{
    while (FreeMem() < MIN_FREE_HEAP && lru_count > 0) {
        lru_evict_one();
    }
}

static void clear_block_caches(void)
{
  int k;
  cache_clear_array(bank0_cache, BANK0_CACHE_SIZE);
  cache_clear_array(upper_cache, UPPER_CACHE_SIZE);
  for (k = 0; k < MAX_ROM_BANKS; k++) {
    if (banked_cache[k]) {
      cache_clear_array(banked_cache[k], BANKED_CACHE_SIZE);
      free(banked_cache[k]);
      banked_cache[k] = NULL;
    }
  }
}

// Called by dmg.c when ROM bank switches
static void on_rom_bank_switch(int new_bank)
{
    jit_ctx.current_rom_bank = (u8) new_bank;
    // Force exit to dispatcher so we use the correct cache
    jit_ctx.interrupt_check = 1;
}

// Flag: JIT hit an error
static int jit_halted = 0;

// Time Manager task for timing and loop interruption
typedef struct {
    long appA5;
    TMTask task;
} TMInfo;

static TMInfo interrupt_tm;

#define INTERRUPT_PERIOD (-16667L)
#define CYCLES_PER_INTERRUPT 70224

struct cpu cpu;
struct rom rom;
struct lcd lcd;
struct dmg dmg;

WindowPtr g_wp;
unsigned char app_running;
unsigned char emulation_on;

static Rect window_bounds = { WINDOW_Y, WINDOW_X, WINDOW_Y + WINDOW_HEIGHT, WINDOW_X + WINDOW_WIDTH };

static char save_filename[32];
// for GetFInfo/SetFInfo
static Str63 save_filename_p;

// 2x scaled: 320x288 @ 1bpp = 40 bytes per row
char offscreen_buf[40 * 288];
Rect offscreen_rect = { 0, 0, 288, 320 };
BitMap offscreen_bmp;

// Status bar - if set, displayed instead of FPS
char status_bar[64];

static void build_save_filename(void)
{
  int len;

  rom_get_title(&rom, save_filename);
  len = strlen(save_filename);

  // build Pascal string
  save_filename_p[0] = len;
  memcpy(&save_filename_p[1], save_filename, len);
}

static pascal void interrupt_tm_proc(void)
{
    asm volatile(
        "move.l %%a5, -(%%sp)\n\t"
        "move.l -4(%%a1), %%a5\n\t"
        ::: "memory"
    );

    jit_ctx.interrupt_check = 1;
    PrimeTime((QElemPtr)&interrupt_tm.task, INTERRUPT_PERIOD);

    asm volatile(
        "move.l (%%sp)+, %%a5\n\t"
        ::: "memory"
    );
}

void InitEverything(void)
{
  Handle mbar;

  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(0L);
  InitCursor();

  MaxApplZone();

  // enable keyUp events (not delivered by default)
  SetEventMask(everyEvent);

  mbar = GetNewMBar(MBAR_DEFAULT);
  SetMenuBar(mbar);
  AppendResMenu(GetMenuHandle(MENU_APPLE), 'DRVR');
  DrawMenuBar();

  app_running = 1;
}

void set_status_bar(const char *str)
{
  int k;
  Str255 pstr;
  Rect statusRect = { 288, 0, 299, 320 };

  if (!strcmp(str, status_bar)) {
    return;
  }

  for (k = 0; k < 63 && str[k]; k++) {
    status_bar[k] = str[k];
  }
  status_bar[k] = '\0';

  EraseRect(&statusRect);
  MoveTo(4, 298);
  for (k = 0; k < 255 && status_bar[k]; k++) {
    pstr[k + 1] = status_bar[k];
  }
  pstr[0] = k;
  DrawString(pstr);
}

// -- JIT EXECUTION --

static void execute_block(void *code)
{
  asm volatile(
    /* Save callee-saved registers */
    "movem.l %%d2-%%d7/%%a2-%%a4, -(%%sp)\n\t"

    /* Copy code pointer to A0 first */
    "movea.l %[code], %%a0\n\t"

    /* Load GB state into 68k registers */
    "move.l %[d4], %%d4\n\t"
    "move.l %[d5], %%d5\n\t"
    "move.l %[d6], %%d6\n\t"
    "move.l %[d7], %%d7\n\t"
    "movea.l %[a2], %%a2\n\t"
    "movea.l %[a3], %%a3\n\t"
    "movea.l %[a4], %%a4\n\t"

    /* Call the generated code */
    "jsr (%%a0)\n\t"

    /* Save results back to memory */
    "move.l %%d0, %[out_d0]\n\t"
    "move.l %%d4, %[out_d4]\n\t"
    "move.l %%d5, %[out_d5]\n\t"
    "move.l %%d6, %[out_d6]\n\t"
    "move.l %%d7, %[out_d7]\n\t"
    "move.l %%a2, %[out_a2]\n\t"
    "move.l %%a3, %[out_a3]\n\t"

    /* Restore callee-saved registers */
    "movem.l (%%sp)+, %%d2-%%d7/%%a2-%%a4\n\t"

    : [out_d0] "=m" (jit_dregs[0]),
      [out_d4] "=m" (jit_dregs[4]),
      [out_d5] "=m" (jit_dregs[5]),
      [out_d6] "=m" (jit_dregs[6]),
      [out_d7] "=m" (jit_dregs[7]),
      [out_a2] "=m" (jit_aregs[2]),
      [out_a3] "=m" (jit_aregs[3])
    : [d4] "m" (jit_dregs[4]),
      [d5] "m" (jit_dregs[5]),
      [d6] "m" (jit_dregs[6]),
      [d7] "m" (jit_dregs[7]),
      [a2] "m" (jit_aregs[2]),
      [a3] "m" (jit_aregs[3]),
      [a4] "m" (jit_aregs[4]),
      [code] "a" (code)
    : "d0", "d1", "d2", "d3", "a0", "a1", "cc", "memory"
  );
}

// Initialize JIT state for a new emulation session
static void jit_init(void)
{
    int k;

    compiler_init();

    // Initialize LRU cache (also clears any old blocks)
    lru_init();
    clear_block_caches();

    // Clear registers
    for (k = 0; k < 8; k++) {
        jit_dregs[k] = 0;
        jit_aregs[k] = 0;
    }

    // Set up context - will be initialized properly in StartEmulation
    jit_ctx.dmg = NULL;
    jit_ctx.read_func = dmg_read;
    jit_ctx.write_func = dmg_write;
    jit_ctx.ei_di_func = dmg_ei_di;
    jit_ctx.interrupt_check = 0;
    jit_ctx.current_rom_bank = 1;  // bank 1 is default after boot
    jit_ctx.bank0_cache = bank0_cache;
    jit_ctx.banked_cache = banked_cache;
    jit_ctx.upper_cache = upper_cache;
    jit_ctx.dispatcher_return = (void *) dispatcher_code;

    jit_halted = 0;
    last_wall_ticks = TickCount();
}

static int jit_step(void)
{
    struct code_block *block;
    unsigned long next_pc;
    char buf[64];
    int took_interrupt;
    u32 finished_ticks;
    u32 wall_ticks;

    if (jit_halted) {
        return 0;
    }

    // Clear interrupt check BEFORE executing so we can make progress
    // even if the timer fired during ProcessEvents
    jit_ctx.interrupt_check = 0;

    // Look up or compile block
    block = cache_lookup(cpu.pc, jit_ctx.current_rom_bank);

    if (!block) {
        u32 free_heap;
        lru_ensure_memory();
        free_heap = (u32) FreeMem();
        sprintf(buf, "Compiling $%02x:%04x free=%u",
                jit_ctx.current_rom_bank, cpu.pc, free_heap);
        set_status_bar(buf);
        block = compile_block(cpu.pc, &compile_ctx);
        if (!block) {
            u32 unused;
            // Try to free memory by evicting LRU blocks
            lru_clear_all();
            clear_block_caches();
            MaxMem(&unused);
            block = compile_block(cpu.pc, &compile_ctx);
            if (!block) {
              sprintf(buf, "JIT: alloc fail pc=%04x", cpu.pc);
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

        debug_log_block(block);
        //if (cpu.pc <= 0x7fff)
        lru_add_block(block, cpu.pc, jit_ctx.current_rom_bank);
    } else {
        // Cache hit - promote in LRU
        if (block->lru_node) {
            lru_promote((lru_node *) block->lru_node);
        }
        set_status_bar("Running");
    }

    execute_block(block->code);

    // Get next PC from D0
    next_pc = jit_dregs[REG_68K_D_NEXT_PC];

    if (next_pc == HALT_SENTINEL) {
        set_status_bar("HALT");
        jit_halted = 1;
        return 0;
    }

    took_interrupt = 0;
    if (cpu.interrupt_enable) {
      u8 pending = dmg.interrupt_enabled & dmg.interrupt_requested & 0x1f;
      if (pending) {
        static const u16 handlers[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
        int k;
        for (k = 0; k < 5; k++) {
          if (pending & (1 << k)) {
            // clear IF bit and disable IME
            dmg.interrupt_requested &= ~(1 << k);
            cpu.interrupt_enable = 0;

            // push PC to stack
            u8 *sp_ptr = (u8 *) jit_aregs[REG_68K_A_SP];
            sp_ptr -= 2;
            sp_ptr[1] = (next_pc >> 8) & 0xff;
            sp_ptr[0] = next_pc & 0xff;
            jit_aregs[REG_68K_A_SP] = (unsigned long) sp_ptr;

            // Jump to handler
            next_pc = handlers[k];
            took_interrupt = 1;
            break;
          }
        }
      }
    }

    cpu.pc = (u16) next_pc;

    finished_ticks = TickCount();
    wall_ticks = finished_ticks - last_wall_ticks;
    last_wall_ticks = finished_ticks;

    // don't sync lcd if interrupt just happened to prevent vblank spam
    // where the "main thread" can't make progress
    if (!took_interrupt) {
        dmg_sync_hw(&dmg, wall_ticks * CYCLES_PER_INTERRUPT);
    }

    return 1;
}

void StartEmulation(void)
{
  if (g_wp) {
    DisposeWindow(g_wp);
    g_wp = NULL;
  }
  g_wp = NewWindow(0, &window_bounds, save_filename_p, true,
        noGrowDocProc, (WindowPtr) -1, true, 0);
  SetPort(g_wp);

  memset(&dmg, 0, sizeof(dmg));
  memset(&cpu, 0, sizeof(cpu));

  if (lcd.pixels) {
    free(lcd.pixels);
  }
  memset(&lcd, 0, sizeof(lcd));
  lcd_new(&lcd);

  dmg_new(&dmg, &cpu, &rom, &lcd);
  dmg.frame_skip = 1;
  dmg.rom_bank_switch_hook = on_rom_bank_switch;
  mbc_load_ram(dmg.rom->mbc, save_filename);

  cpu.dmg = &dmg;
  cpu.pc = 0x100;

  offscreen_bmp.baseAddr = offscreen_buf;
  offscreen_bmp.bounds = offscreen_rect;
  offscreen_bmp.rowBytes = 40;

  // Initialize JIT
  jit_init();
  jit_ctx.dmg = &dmg;
  jit_aregs[REG_68K_A_SP] = (unsigned long) (dmg.zero_page + 0xfffe - 0xff80);
  jit_aregs[REG_68K_A_CTX] = (unsigned long) &jit_ctx;

  // Initialize compile-time context
  compile_ctx.dmg = &dmg;
  compile_ctx.read = dmg_read;
  compile_ctx.wram_base = dmg.main_ram;
  compile_ctx.hram_base = dmg.zero_page;

  emulation_on = 1;
}

int LoadRom(Str63 fileName, short vRefNum)
{
  int err;
  short fileNo;
  long amtRead;
  FInfo fndrInfo;
  
  if(rom.data != NULL) {
    // unload existing ROM
    free((char *) rom.data);
    rom.length = 0;
  }

  err = FSOpen(fileName, vRefNum, &fileNo);
  
  if(err != noErr) {
    return false;
  }
  
  GetEOF(fileNo, (long *) &rom.length);
  rom.data = (unsigned char *) malloc(rom.length);
  if(rom.data == NULL) {
    ShowCenteredAlert(ALRT_NOT_ENOUGH_RAM, "\p", "\p", "\p", "\p", ALERT_NORMAL);
    return false;
  }
  
  amtRead = rom.length;
  FSRead(fileNo, &amtRead, rom.data);
  FSClose(fileNo);

  rom.mbc = mbc_new(rom.data[0x147]);
  if (!rom.mbc) {
    ShowCenteredAlert(
        ALRT_4_LINE,
        "\pThis cartridge type is unsupported.", "\p", "\p", "\p",
        ALERT_NORMAL
    );
    return false;
  }

  if (GetFInfo(fileName, vRefNum, &fndrInfo) == noErr) {
    fndrInfo.fdType = 'GBRM';
    fndrInfo.fdCreator = 'MGBE';
    SetFInfo(fileName, vRefNum, &fndrInfo);
  }

  build_save_filename();

  return true;
}

// -- EVENT FUNCTIONS --

void OnMenuAction(long action)
{
  short menu, item;
  
  if(action <= 0)
    return;

  HiliteMenu(0);
  
  menu = HiWord(action);
  item = LoWord(action);
  
  if(menu == MENU_APPLE) {
    if(item == APPLE_ABOUT) {
      ShowAboutBox();
    } else {
      Str255 daName;
      GetMenuItemText(GetMenuHandle(MENU_APPLE), item, daName);
      OpenDeskAcc(daName);
    }
  }
  
  else if(menu == MENU_FILE) {
    if(item == FILE_OPEN) {
      if(ShowOpenBox())
        StartEmulation();
    }
    else if(item == FILE_QUIT) {
      app_running = 0;
    }
  }

  else if (menu == MENU_EDIT) {
    if (item == EDIT_PREFERENCES || item == EDIT_KEY_MAPPINGS) {
      ShowCenteredAlert(
          ALRT_4_LINE,
          "\pThis feature is not yet implemented.", "\p", "\p", "\p",
          ALERT_NORMAL
      );
    }
  }
}

void OnMouseDown(EventRecord *pEvt)
{
  short part;
  WindowPtr clicked;
  long action;
  
  part = FindWindow(pEvt->where, &clicked);
  
  switch(part) {
    case inDrag:
      DragWindow(clicked, pEvt->where, &qd.screenBits.bounds);
      break;
    case inGoAway:
      if(TrackGoAway(clicked, pEvt->where)) {
        emulation_on = 0;
        DisposeWindow(clicked);
        g_wp = NULL;
      }
      break;
    case inContent:
      if(clicked != FrontWindow())
        SelectWindow(clicked);
      break;
    case inMenuBar:
      action = MenuSelect(pEvt->where);
      OnMenuAction(action);
      break;
  }
}

// process pending events, returns 0 if app should quit
static int ProcessEvents(void)
{
  EventRecord evt;

  while (GetNextEvent(everyEvent, &evt)) {
    switch (evt.what) {
      case mouseDown:
        OnMouseDown(&evt);
        break;
      case updateEvt:
        BeginUpdate((WindowPtr) evt.message);
        EndUpdate((WindowPtr) evt.message);
        break;
      case keyDown:
      case autoKey:
        if (evt.modifiers & cmdKey) {
          OnMenuAction(MenuKey(evt.message & charCodeMask));
        } else if (emulation_on) {
          HandleKeyEvent(evt.message & charCodeMask, 1);
        }
        break;
      case keyUp:
        if (emulation_on) {
          HandleKeyEvent(evt.message & charCodeMask, 0);
        }
        break;
    }

    if (!app_running) {
      return 0;
    }
  }

  return 1;
}

// check for files passed from Finder on launch
// returns 1 if ROM loaded, 0 if should show open dialog
static int CheckFinderFiles(void)
{
  short action, count;
  AppFile theFile;

  CountAppFiles(&action, &count);
  if (count == 0) {
    return 0;
  }

  GetAppFiles(1, &theFile);

  if (theFile.fType == 'GBRM') {
    if (LoadRom(theFile.fName, theFile.vRefNum)) {
      ClrAppFiles(1);
      return 1;
    }
  } else if (theFile.fType == 'SRAM') {
    ShowCenteredAlert(
        ALRT_4_LINE,
        "\pSave files cannot be opened directly.",
        "\pOpen the ROM instead, and the save",
        "\pwill be loaded automatically.",
        "\p",
        ALERT_CAUTION
    );
    ClrAppFiles(1);
    return 0;
  }

  ClrAppFiles(1);
  return 0;
}

// -- ENTRY POINT --
int main(int argc, char *argv[])
{
  unsigned int frame_count = 0;
  unsigned int last_ticks = 0;
  int finderResult;

  InitEverything();
  init_dither_lut();

  // Install Time Manager task for timing and loop interruption
  interrupt_tm.appA5 = (long) SetCurrentA5();
  interrupt_tm.task.tmAddr = (TimerProcPtr) interrupt_tm_proc;
  InsTime((QElemPtr) &interrupt_tm.task);
  PrimeTime((QElemPtr) &interrupt_tm.task, INTERRUPT_PERIOD);

  finderResult = CheckFinderFiles();
  if (finderResult == 1) {
    StartEmulation();
  } else if (ShowOpenBox()) {
    StartEmulation();
  }

  while (app_running) {
    unsigned int now;

    if (!ProcessEvents()) {
      break;
    }

    if (emulation_on) {
      // JIT mode: run blocks until timer fires
      jit_step();
      frame_count++;
    }
  }
  if (mbc_save_ram(dmg.rom->mbc, save_filename)) {
    FInfo fndrInfo;
    if (GetFInfo(save_filename_p, 0, &fndrInfo) == noErr) {
      fndrInfo.fdType = 'SRAM';
      fndrInfo.fdCreator = 'MGBE';
      SetFInfo(save_filename_p, 0, &fndrInfo);
    }
  }
  RmvTime((QElemPtr) &interrupt_tm.task);

  return 0;
}
