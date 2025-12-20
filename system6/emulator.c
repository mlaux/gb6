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

#include "emulator.h"

#include "dmg.h"
#include "cpu.h"
#include "rom.h"
#include "lcd.h"
#include "mbc.h"
#include "compiler.h"

// compiler infrastructure

#define MAX_CACHED_BLOCKS 65536
#define HALT_SENTINEL 0xffffffff

// block cache indexed by GB PC
static struct code_block *block_cache[MAX_CACHED_BLOCKS];

// Debug logging - open/close each time to avoid losing data on crash
static int debug_enabled = 1;

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
typedef struct {
    void *dmg;
    void *read_func;
    void *write_func;
    void *ei_di_func;
} jit_context;

static jit_context jit_ctx;

// Compile-time context for address calculation
static struct compile_ctx compile_ctx;

// Flag: 0 = interpreter, 1 = JIT
static int use_jit = 1;

// Flag: JIT hit an error
static int jit_halted = 0;

WindowPtr g_wp;
DialogPtr stateDialog;
unsigned char g_running;
unsigned char emulationOn;

// key input mapping for game controls
struct key_input {
    char key;
    int button;
    int field;
};

static struct key_input key_inputs[] = {
    { 'd', BUTTON_RIGHT, FIELD_JOY },
    { 'a', BUTTON_LEFT, FIELD_JOY },
    { 'w', BUTTON_UP, FIELD_JOY },
    { 's', BUTTON_DOWN, FIELD_JOY },
    { 'l', BUTTON_A, FIELD_ACTION },
    { 'k', BUTTON_B, FIELD_ACTION },
    { 'n', BUTTON_SELECT, FIELD_ACTION },
    { 'm', BUTTON_START, FIELD_ACTION },
    { 0, 0, 0 }
};

static Point windowPt = { WINDOW_Y, WINDOW_X };

static Rect windowBounds = { WINDOW_Y, WINDOW_X, WINDOW_Y + WINDOW_HEIGHT, WINDOW_X + WINDOW_WIDTH };

struct cpu cpu;
struct rom rom;
struct lcd lcd;
struct dmg dmg;

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

  // enable keyUp events (not delivered by default)
  SetEventMask(everyEvent);

  mbar = GetNewMBar(MBAR_DEFAULT);
  SetMenuBar(mbar);
  DrawMenuBar();

  g_running = 1;
}

// 2x scaled: 320x288 @ 1bpp = 40 bytes per row
char offscreen[40 * 288];
Rect offscreenRect = { 0, 0, 288, 320 };

// FPS tracking
static unsigned long fps_frame_count = 0;
static unsigned long fps_last_tick = 0;
static unsigned long fps_display = 0;

// Status bar - if set, displayed instead of FPS
static char status_bar[64] = "";

void set_status_bar(const char *str)
{
  int k;
  for (k = 0; k < 63 && str[k]; k++) {
    status_bar[k] = str[k];
  }
  status_bar[k] = '\0';
}

BitMap offscreenBmp;

// lookup tables for 2x dithered rendering
// index = 4 packed GB pixels (2 bits each), output = 8 screen pixels (1bpp)
static unsigned char dither_row0[256];
static unsigned char dither_row1[256];

void init_dither_lut(void)
{
  // 2x2 dither patterns for each GB color (0-3)
  // stored in high 2 bits: 00=white, 01=right black, 10=left black, 11=both black
  // color 0 (white):      top=00 bot=00
  // color 1 (light gray): top=00 bot=01 (one black pixel, bottom-right)
  // color 2 (dark gray):  top=10 bot=01 (checkerboard)
  // color 3 (black):      top=11 bot=11
  static const unsigned char pat_top[4] = { 0x00, 0x00, 0x80, 0xc0 };
  static const unsigned char pat_bot[4] = { 0x00, 0x40, 0x40, 0xc0 };
  int idx;

  for (idx = 0; idx < 256; idx++) {
    int p0 = (idx >> 6) & 3;
    int p1 = (idx >> 4) & 3;
    int p2 = (idx >> 2) & 3;
    int p3 = idx & 3;

    dither_row0[idx] = pat_top[p0] | (pat_top[p1] >> 2) |
                       (pat_top[p2] >> 4) | (pat_top[p3] >> 6);
    dither_row1[idx] = pat_bot[p0] | (pat_bot[p1] >> 2) |
                       (pat_bot[p2] >> 4) | (pat_bot[p3] >> 6);
  }
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

    // Clear block cache
    for (k = 0; k < MAX_CACHED_BLOCKS; k++) {
        if (block_cache[k]) {
            block_free(block_cache[k]);
            block_cache[k] = NULL;
        }
    }

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

    jit_halted = 0;
}

// Execute one JIT block, returns cycles consumed (0 if halted/error)
static int jit_step(void)
{
    struct code_block *block;
    unsigned long next_pc;
    char buf[64];

    if (jit_halted) {
        return 0;
    }

    // Look up or compile block
    // if (cpu.pc < MAX_CACHED_BLOCKS) {
        block = block_cache[cpu.pc];
    // } else {
    //     block = NULL;
    // }

    if (!block) {
        block = compile_block(cpu.pc, &compile_ctx);
        if (!block) {
            sprintf(buf, "JIT: alloc fail pc=%04x", cpu.pc);
            set_status_bar(buf);

            lcd_draw(dmg.lcd);
            jit_halted = 1;
            return 0;
        }

        // Check for compilation error
        if (block->error) {
            sprintf(buf, "pc=%04x op=%02x", block->failed_address, block->failed_opcode);
            set_status_bar(buf);

            lcd_draw(dmg.lcd);
            jit_halted = 1;
            block_free(block);
            return 0;
        }


        // Cache the block
        // if (cpu.pc < MAX_CACHED_BLOCKS) {
            block_cache[cpu.pc] = block;
        // }
    }

    // Log the compiled block
    debug_log_block(block);
    // Execute the block
    execute_block(block->code);

    // Get next PC from D0
    next_pc = jit_dregs[REG_68K_D_NEXT_PC];

    if (next_pc == HALT_SENTINEL) {
        set_status_bar("HALT");
        jit_halted = 1;
        return 0;
    }

    // Debug: show interrupt state every ~1000 blocks
    {
        static int dbg_cnt = 0;
        static u16 min_pc = 0xffff, max_pc = 0;
        if (cpu.pc < min_pc) min_pc = cpu.pc;
        if (cpu.pc > max_pc) max_pc = cpu.pc;
        if (++dbg_cnt >= 1000) {
            dbg_cnt = 0;
            sprintf(buf, "PC=%04x-%04x IE=%02x", min_pc, max_pc,
                    dmg.interrupt_enabled);
            set_status_bar(buf);
            lcd_draw(dmg.lcd);
            min_pc = 0xffff;
            max_pc = 0;
        }
    }

    // Check for pending interrupts
    if (cpu.interrupt_enable) {
      u8 pending = dmg.interrupt_enabled & dmg.interrupt_requested & 0x1f;
      if (pending) {
        static const u16 handlers[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
        int k;
        for (k = 0; k < 5; k++) {
          if (pending & (1 << k)) {
            sprintf(buf, "INT %d -> %04x", k, handlers[k]);
            set_status_bar(buf);
            lcd_draw(dmg.lcd);
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
            break;
          }
        }
      }
    }

    // Update CPU state
    cpu.pc = (u16) next_pc;

    // this is never actually set by the compiler lol so it'll always be 16
    // for now
    cpu.cycle_count += block->gb_cycles ? block->gb_cycles : 16;

    // Sync LCD/timer state
    dmg_sync_hw(&dmg);

    return 1;
}

// called by dmg_step at vblank
void lcd_draw(struct lcd *lcd_ptr)
{
  int gy;
  unsigned char *src = lcd_ptr->pixels;
  unsigned char *dst = (unsigned char *) offscreen;

  for (gy = 0; gy < 144; gy++) {
    unsigned char *row0 = dst;
    unsigned char *row1 = dst + 40;
    int gx;

    for (gx = 0; gx < 160; gx += 4) {
      // pack 4 GB pixels into LUT index
      unsigned char idx = (src[0] << 6) | (src[1] << 4) | (src[2] << 2) | src[3];
      src += 4;

      *row0++ = dither_row0[idx];
      *row1++ = dither_row1[idx];
    }

    dst += 80; // advance 2 screen rows
  }

  SetPort(g_wp);
  CopyBits(&offscreenBmp, &g_wp->portBits, &offscreenRect, &offscreenRect, srcCopy, NULL);

  // FPS display
  {
    unsigned long now = TickCount();
    Str255 fpsStr;

    fps_frame_count++;
    if (now - fps_last_tick >= 60) {
      fps_display = fps_frame_count;
      fps_frame_count = 0;
      fps_last_tick = now;
    }

    {
      Rect statusRect = { 288, 0, 299, 320 };
      EraseRect(&statusRect);
    }
    MoveTo(4, 298);

    if (status_bar[0]) {
      // Convert C string to Pascal string and draw
      Str255 pstr;
      int k;
      for (k = 0; k < 255 && status_bar[k]; k++) {
        pstr[k + 1] = status_bar[k];
      }
      pstr[0] = k;
      DrawString(pstr);
    } else {
      NumToString(fps_display, fpsStr);
      DrawString("\pFPS: ");
      DrawString(fpsStr);
    }
  }
}

void StartEmulation(void)
{
  if (g_wp) {
    DisposeWindow(g_wp);
    g_wp = NULL;
  }
  g_wp = NewWindow(0, &windowBounds, WINDOW_TITLE, true,
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
  mbc_load_ram(dmg.rom->mbc, "save.sav");

  cpu.dmg = &dmg;
  cpu.pc = 0x100;

  offscreenBmp.baseAddr = offscreen;
  offscreenBmp.bounds = offscreenRect;
  offscreenBmp.rowBytes = 40;

  // Initialize JIT
  jit_init();
  jit_ctx.dmg = &dmg;
  jit_aregs[REG_68K_A_CTX] = (unsigned long) &jit_ctx;

  // Initialize compile-time context
  compile_ctx.dmg = &dmg;
  compile_ctx.read = dmg_read;
  compile_ctx.wram_base = dmg.main_ram;
  compile_ctx.hram_base = dmg.zero_page;

  emulationOn = 1;
}

int LoadRom(Str63 fileName, short vRefNum)
{
  int err;
  short fileNo;
  long amtRead;
  
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
    Alert(ALRT_NOT_ENOUGH_RAM, NULL);
    return false;
  }
  
  amtRead = rom.length;
  FSRead(fileNo, &amtRead, rom.data);
  FSClose(fileNo);

  rom.mbc = mbc_new(rom.data[0x147]);
  if (!rom.mbc) {
    ParamText("\pThis cartridge type is unsupported.", "\p", "\p", "\p");
    Alert(ALRT_4_LINE, NULL);
    return false;
  }

  return true;
}

// -- DIALOG BOX FUNCTIONS --

int ShowOpenBox(void)
{
  SFReply reply;
  Point pt = { 0, 0 };
  const int stdWidth = 348;
  Rect rect;
  
  pt.h = qd.screenBits.bounds.right / 2 - stdWidth / 2;
  
  SFGetFile(pt, NULL, NULL, -1, NULL, NULL, &reply);
  
  if(reply.good) {
    return LoadRom(reply.fName, reply.vRefNum);
  }
  
  return false;
}

void ShowStateDialog(void)
{
  DialogPtr dp;

  if (!stateDialog) {
    stateDialog = GetNewDialog(DLOG_STATE, 0L, (WindowPtr) -1L);
  }

  ShowWindow(stateDialog);
  SelectWindow(stateDialog);
}

void ShowAboutBox(void)
{
  DialogPtr dp;
  EventRecord e;
  DialogItemIndex hitItem;
  
  dp = GetNewDialog(DLOG_ABOUT, 0L, (WindowPtr) -1L);
  
  ModalDialog(NULL, &hitItem);

  // DrawDialog(dp);
  // while(!GetNextEvent(mDownMask, &e));
  // while(WaitMouseUp());

  DisposeDialog(dp);
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
    }	
  }
  
  else if(menu == MENU_FILE) {
    if(item == FILE_OPEN) {
      if(ShowOpenBox())
        StartEmulation();
    }
    else if(item == FILE_QUIT) {
      g_running = 0;
    }
  }

  else if (menu == MENU_EMULATION) {
    if (item == EMULATION_STATE) {
      ShowStateDialog();
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
        emulationOn = 0;
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

static void HandleKeyEvent(int ch, int down)
{
  struct key_input *key = key_inputs;

  // idk if this is needed? does caps lock make the char capitalized?
  if (ch >= 'A' && ch <= 'Z') ch += 32;

  while (key->key) {
    if (key->key == ch) {
      dmg_set_button(&dmg, key->field, key->button, down);
      break;
    }
    key++;
  }
}

// process pending events, returns 0 if app should quit
static int ProcessEvents(void)
{
  EventRecord evt;

  while (GetNextEvent(everyEvent, &evt)) {
    if (IsDialogEvent(&evt)) {
      DialogRef hitBox;
      DialogItemIndex hitItem;
      if (DialogSelect(&evt, &hitBox, &hitItem)) {
        stateDialog = NULL;
      }
    } else switch (evt.what) {
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
        } else if (emulationOn) {
          HandleKeyEvent(evt.message & charCodeMask, 1);
        }
        break;
      case keyUp:
        if (emulationOn) {
          HandleKeyEvent(evt.message & charCodeMask, 0);
        }
        break;
    }

    if (!g_running) {
      return 0;
    }
  }

  return 1;
}

// -- ENTRY POINT --
int main(int argc, char *argv[])
{
  unsigned int frame_count = 0;
  unsigned int last_ticks = 0;

  InitEverything();
  init_dither_lut();
  if(ShowOpenBox()) {
    StartEmulation();
  }

  while (g_running) {
    unsigned int now;

    if (!ProcessEvents()) {
      break;
    }

    if (emulationOn && (now = TickCount()) != last_ticks) {
      last_ticks = now;
      // run one frame (70224 cycles = 154 scanlines * 456 cycles each
      unsigned long frame_end = cpu.cycle_count + 70224;

      if (use_jit) {
        // JIT mode: compile and execute blocks
        while ((long) (frame_end - cpu.cycle_count) > 0 && !jit_halted) {
          jit_step();
        }
      } else {
        // Interpreter mode
        while ((long) (frame_end - cpu.cycle_count) > 0) {
          dmg_step(&dmg);
        }
      }
      frame_count++;
    }
  }
  mbc_save_ram(dmg.rom->mbc, "save.sav");

  return 0;
}
