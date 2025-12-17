/* Game Boy emulator for 68k Macs
   emulator.c - entry point */

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <Quickdraw.h>
#include <StandardFile.h>
#include <Dialogs.h>
#include <Menus.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <Memory.h>

#include "emulator.h"

#include "dmg.h"
#include "cpu.h"
#include "rom.h"
#include "lcd.h"
#include "mbc.h"

WindowPtr g_wp;
DialogPtr stateDialog;
unsigned char g_running;
unsigned char emulationOn;

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
  
  mbar = GetNewMBar(MBAR_DEFAULT);
  SetMenuBar(mbar);
  DrawMenuBar();
  
  g_running = 1;
}

// 2x scaled: 320x288 @ 1bpp = 40 bytes per row
char offscreen[40 * 288];
Rect offscreenRect = { 0, 0, 288, 320 };

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
}

// with interpreter and 8 MHz 68000:
// 10000 instructions
// 417 ticks

// 0.0417 tick per instruction
// 1/60 second per tick
// 0.000695 second per instruction
// 0.695 ms per instruction

// REAL GB:
// 4194304 Hz
// 1048576 NOPs per second
// 174763 CALL 16s per second
// 0.001 ms per NOP
// 0.006 ms per CALL 16

void StartEmulation(void)
{
  g_wp = NewWindow(0, &windowBounds, WINDOW_TITLE, true,
        noGrowDocProc, (WindowPtr) -1, true, 0);
  SetPort(g_wp);

  offscreenBmp.baseAddr = offscreen;
  offscreenBmp.bounds = offscreenRect;
  offscreenBmp.rowBytes = 40;
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
      if(TrackGoAway(clicked, pEvt->where))
        DisposeWindow(clicked);
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
        if (evt.modifiers & cmdKey) {
          OnMenuAction(MenuKey(evt.message & charCodeMask));
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

  lcd_new(&lcd);
  dmg_new(&dmg, &cpu, &rom, &lcd);
  cpu.dmg = &dmg;
  cpu.pc = 0x100;

  while (g_running) {
    unsigned int now;

    if (!ProcessEvents()) {
      break;
    }

    if (emulationOn && (now = TickCount()) != last_ticks) {
      last_ticks = now;
      // run one frame (70224 cycles = 154 scanlines * 456 cycles each
      unsigned long frame_end = cpu.cycle_count + 70224;
      while ((long) (frame_end - cpu.cycle_count) > 0) {
        dmg_step(&dmg);
      }
      frame_count++;
    }
  }

  return 0;
}
