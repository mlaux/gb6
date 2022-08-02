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

// 160 pixels / 8 bits per pixel = 20 bytes per line
char offscreen[20 * 144];
Rect offscreenRect = { 0, 0, 144, 160 };

BitMap offscreenBmp;

int execTime;

void Render(void)
{
  // long k = 0, dst, bit;
  // for (dst = 0; dst < 20 * 144; dst++) {
  //   for (bit = 7; bit >= 0; bit--) {
  //     offscreen[dst] |= lcd.pixels[k++];
  //     offscreen[dst] <<= 1;
  //   }
  //   dst++;
  // }

  /*
    offscreen[dst] = lcd.pixels[k++] << 7;
    offscreen[dst] |= lcd.pixels[k++] << 6;
    offscreen[dst] |= lcd.pixels[k++] << 5;
    offscreen[dst] |= lcd.pixels[k++] << 4;
    offscreen[dst] |= lcd.pixels[k++] << 3;
    offscreen[dst] |= lcd.pixels[k++] << 2;
    offscreen[dst] |= lcd.pixels[k++] << 1;
    offscreen[dst] |= lcd.pixels[k++];
  */
  SetPort(g_wp);
  // CopyBits(&offscreenBmp, &g_wp->portBits, &offscreenRect, &offscreenRect, srcCopy, NULL);

  EraseRect(&g_wp->portRect);
  MoveTo(10, 180);
  char debug[128];
  double ms = execTime / 600.0;
  sprintf(debug, "10000 in %d ticks, %.2f ms per instruction", execTime, ms);
  C2PStr(debug);
  DrawString(debug);
}

// 417 ticks
// 10000 instructions

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
  offscreenBmp.rowBytes = 20;
  emulationOn = 1;
}

bool LoadRom(StrFileName fileName, short vRefNum)
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

bool ShowOpenBox(void)
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

// -- ENTRY POINT --
int main(int argc, char *argv[])
{
  EventRecord evt;

  int executed;
  int paused = 0;
  int pause_next = 0;

  InitEverything();

  lcd_new(&lcd);
  dmg_new(&dmg, &cpu, &rom, &lcd);
  cpu.dmg = &dmg;
  // cpu_bind_mem_model(&cpu, &dmg, dmg_read, dmg_write);

  cpu.pc = 0x100;

  while(g_running) {
    if (emulationOn) {
      int k;
      int start = TickCount();
      for (k = 0; k < 10000; k++) {
        dmg_step(&dmg);
      }
      execTime = TickCount() - start;
      emulationOn = false;
      Render();
    } else {
      if(WaitNextEvent(everyEvent, &evt, 0, 0) != nullEvent) {
        if (IsDialogEvent(&evt)) {
          DialogRef hitBox;
          DialogItemIndex hitItem;
          if (DialogSelect(&evt, &hitBox, &hitItem)) {
            stateDialog = NULL;
          }
        } else switch(evt.what) {
          case mouseDown:
            OnMouseDown(&evt);
            break;
          case updateEvt:
            BeginUpdate((WindowPtr) evt.message);
            Render();
            EndUpdate((WindowPtr) evt.message);
            break;
        }
      }
    }
  }
  
  return 0;
}
