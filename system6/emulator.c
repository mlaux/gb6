/* Game Boy emulator for 68k Macs
   emulator.c - entry point */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Memory.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <Timer.h>
#include <Files.h>
#include <SegLoad.h>

#include "emulator.h"

#include "cpu.h"
#include "dmg.h"
#include "lcd.h"
#include "rom.h"
#include "mbc.h"

#include "debug.h"
#include "dialogs.h"
#include "input.h"
#include "lcd_mac.h"
#include "dispatcher_asm.h"
#include "lru.h"
#include "jit.h"
#include "settings.h"

#include "compiler.h"

// Called by dmg.c when ROM bank switches
static void on_rom_bank_switch(int new_bank)
{
    jit_ctx.current_rom_bank = (u8) new_bank;
    // force exit to dispatcher ?
    // only way this is needed is if games switch banks and then don't jump
    // or call afterwards... 
}

// this is pretty much hollowed out and a lot of it can go.
// from the interpreter version
struct cpu cpu;
struct rom rom;
struct lcd lcd;
struct dmg dmg;

WindowPtr g_wp;
unsigned char app_running;
unsigned char emulation_on;
int screen_depth;

static unsigned long soft_reset_release_tick;

static Rect window_bounds = { WINDOW_Y, WINDOW_X, WINDOW_Y + WINDOW_HEIGHT, WINDOW_X + WINDOW_WIDTH };

static char save_filename[32];
// for GetFInfo/SetFInfo
static Str63 save_filename_p;

// 2x scaled: 320x288 @ 1bpp = 40 bytes per row
char offscreen_buf[40 * 288];
Rect offscreen_rect = { 0, 0, 288, 320 };
BitMap offscreen_bmp;

// color/grayscale mode: 320x288 @ 8bpp
char offscreen_color_buf[320 * 288];
PixMap offscreen_pixmap;
CTabHandle offscreen_ctab;

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
  InsertMenuItem(GetMenuHandle(MENU_FILE), "\pSoft Reset", FILE_SCREENSHOT);
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

void DetectScreenDepth(void)
{
  SysEnvRec env;
  screen_depth = 1;

  // check if Color QuickDraw is available
  if (SysEnvirons(1, &env) == noErr && env.hasColorQD) {
    GDHandle mainDev = GetMainDevice();
    if (mainDev) {
      PixMapHandle pm = (*mainDev)->gdPMap;
      screen_depth = (*pm)->pixelSize;
    }
  }
}

void InitColorOffscreen(void)
{
  GDHandle mainDev;
  PixMapHandle screenPM;

  // use screen's color table so CopyBits skips color matching
  mainDev = GetMainDevice();
  screenPM = (*mainDev)->gdPMap;
  offscreen_ctab = (*screenPM)->pmTable;

  // PixMap setup - always 8bpp, CopyBits handles depth conversion
  offscreen_pixmap.baseAddr = offscreen_color_buf;
  offscreen_pixmap.rowBytes = 320 | 0x8000;  // high bit = PixMap flag
  offscreen_pixmap.bounds = offscreen_rect;
  offscreen_pixmap.pmVersion = 0;
  offscreen_pixmap.packType = 0;
  offscreen_pixmap.packSize = 0;
  offscreen_pixmap.hRes = 0x00480000;  // 72 dpi
  offscreen_pixmap.vRes = 0x00480000;
  offscreen_pixmap.pixelType = 0;  // chunky
  offscreen_pixmap.pixelSize = 8;
  offscreen_pixmap.cmpCount = 1;
  offscreen_pixmap.cmpSize = 8;
  offscreen_pixmap.pmTable = offscreen_ctab;
  offscreen_pixmap.pmReserved = 0;
}

void StartEmulation(void)
{
  if (g_wp) {
    DisposeWindow(g_wp);
    g_wp = NULL;
  }

  if (screen_depth > 1) {
    g_wp = NewCWindow(0, &window_bounds, save_filename_p, true,
          noGrowDocProc, (WindowPtr) -1, true, 0);
  } else {
    g_wp = NewWindow(0, &window_bounds, save_filename_p, true,
          noGrowDocProc, (WindowPtr) -1, true, 0);
  }
  SetPort(g_wp);

  memset(&dmg, 0, sizeof(dmg));
  memset(&cpu, 0, sizeof(cpu));

  if (lcd.pixels) {
    free(lcd.pixels);
  }
  memset(&lcd, 0, sizeof(lcd));
  lcd_new(&lcd);

  dmg_new(&dmg, &cpu, &rom, &lcd);
  // +1 because it's actually (frames % skip == 0)
  dmg.frame_skip = frame_skip + 1;
  dmg.rom_bank_switch_hook = on_rom_bank_switch;
  mbc_load_ram(dmg.rom->mbc, save_filename);

  cpu.dmg = &dmg;

  if (screen_depth > 1) {
    InitColorOffscreen();
  } else {
    offscreen_bmp.baseAddr = offscreen_buf;
    offscreen_bmp.bounds = offscreen_rect;
    offscreen_bmp.rowBytes = 40;
  }

  jit_init(&dmg);
  emulation_on = 1;
  DisableItem(GetMenuHandle(MENU_EDIT), EDIT_PREFERENCES);
}

void CheckSoftResetRelease(void)
{
  if (soft_reset_release_tick && TickCount() >= soft_reset_release_tick) {
    dmg_set_button(&dmg, FIELD_ACTION,
        BUTTON_A | BUTTON_B | BUTTON_SELECT | BUTTON_START, 0);
    soft_reset_release_tick = 0;
  }
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
    else if(item == FILE_SCREENSHOT) {
      if (emulation_on) {
        SaveScreenshot();
      }
    }
    else if(item == FILE_SOFT_RESET) {
      if (emulation_on) {
        dmg_set_button(&dmg, FIELD_ACTION,
            BUTTON_A | BUTTON_B | BUTTON_SELECT | BUTTON_START, 1);
        soft_reset_release_tick = TickCount() + SOFT_RESET_TICKS;
      }
    }
    else if(item == FILE_QUIT) {
      app_running = 0;
    }
  }

  else if (menu == MENU_EDIT) {
    if (item == EDIT_KEY_MAPPINGS) {
      ShowKeyMappingsDialog();
    } else if (item == EDIT_PREFERENCES) {
      ShowPreferencesDialog();
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
        EnableItem(GetMenuHandle(MENU_EDIT), EDIT_PREFERENCES);
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
          int key = (evt.message & keyCodeMask) >> 8;
          HandleKeyEvent(key, 1);
        }
        break;
      case keyUp:
        if (emulation_on) {
          int key = (evt.message & keyCodeMask) >> 8;
          HandleKeyEvent(key, 0);
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

int main(int argc, char *argv[])
{
  int finderResult;

  InitEverything();
  DetectScreenDepth();
  LoadKeyMappings();
  LoadPreferences();

  init_dither_lut();
  if (screen_depth > 1) {
    // init even if indexed isn;t currently selected so it's correct
    // if they change to indexed in settings
    init_indexed_lut();
  }
  lcd_init_lut();

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
      CheckSoftResetRelease();
      jit_step(&dmg);
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

  return 0;
}
