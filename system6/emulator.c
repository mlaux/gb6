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
#include <Files.h>
#include <SegLoad.h>
#include <Resources.h>

#include "emulator.h"

#include "dmg.h"
#include "cpu.h"
#include "rom.h"
#include "lcd.h"
#include "mbc.h"

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

// FPS tracking
static unsigned long fps_frame_count = 0;
static unsigned long fps_last_tick = 0;
static unsigned long fps_display = 0;

// lookup tables for 2x dithered rendering
// index = 4 packed GB pixels (2 bits each), output = 8 screen pixels (1bpp)
static unsigned char dither_row0[256];
static unsigned char dither_row1[256];

// key input mapping for game controls
// using ASCII character codes (case-insensitive handled in code)
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

  // enable keyUp events (not delivered by default)
  SetEventMask(everyEvent);

  mbar = GetNewMBar(MBAR_DEFAULT);
  SetMenuBar(mbar);
  AppendResMenu(GetMenuHandle(MENU_APPLE), 'DRVR');
  DrawMenuBar();

  app_running = 1;
}

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
  unsigned char *dst = (unsigned char *) offscreen_buf;

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
  CopyBits(&offscreen_bmp, &g_wp->portBits, &offscreen_rect, &offscreen_rect, srcCopy, NULL);

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

    NumToString(fps_display, fpsStr);
    {
      Rect fpsRect = { 288, 0, 299, 60 };
      EraseRect(&fpsRect);
    }
    MoveTo(4, 298);
    DrawString("\pFPS: ");
    DrawString(fpsStr);
  }
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
  mbc_load_ram(dmg.rom->mbc, save_filename);

  cpu.dmg = &dmg;
  cpu.pc = 0x100;

  offscreen_bmp.baseAddr = offscreen_buf;
  offscreen_bmp.bounds = offscreen_rect;
  offscreen_bmp.rowBytes = 40;
  emulation_on = 1;
}

typedef short (*AlertProc)(short alertID);

static short AlertWrapper(short alertID) { return Alert(alertID, NULL); }
static short CautionAlertWrapper(short alertID) { return CautionAlert(alertID, NULL); }
static short NoteAlertWrapper(short alertID) { return NoteAlert(alertID, NULL); }
static short StopAlertWrapper(short alertID) { return StopAlert(alertID, NULL); }

static short ShowCenteredAlert(
    short alertID,
    const char *s0,
    const char *s1,
    const char *s2,
    const char *s3,
    AlertProc alertProc
) {
  Handle alrt;
  Rect *bounds;
  Rect screen;
  short width, height, dh, dv;

  ParamText(s0, s1, s2, s3);

  alrt = GetResource('ALRT', alertID);
  if (alrt == nil) {
    return alertProc(alertID);
  }

  bounds = (Rect *) *alrt;
  screen = qd.screenBits.bounds;
  screen.top += GetMBarHeight();

  width = bounds->right - bounds->left;
  height = bounds->bottom - bounds->top;

  /* center horizontally, position 1/4 down vertically */
  dh = ((screen.right - screen.left) - width) / 2 - bounds->left;
  dv = ((screen.bottom - screen.top) - height) / 4 - bounds->top + screen.top;

  OffsetRect(bounds, dh, dv);

  return alertProc(alertID);
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
    ShowCenteredAlert(ALRT_NOT_ENOUGH_RAM, "\p", "\p", "\p", "\p", AlertWrapper);
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
        AlertWrapper
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

// -- DIALOG BOX FUNCTIONS --

// return true to hide, false to show
static pascal Boolean RomFileFilter(CInfoPBRec *pb)
{
  StringPtr name;
  unsigned char len;

  // always show 'GBRM' files
  if (pb->hFileInfo.ioFlFndrInfo.fdType == 'GBRM') {
    return false;
  }

  // show files ending in .gb or .GB (for imports)
  name = pb->hFileInfo.ioNamePtr;
  len = name[0];
  if (len >= 3) {
    char c1 = name[len - 2];
    char c2 = name[len - 1];
    char c3 = name[len];
    if (c1 == '.' && (c2 == 'g' || c2 == 'G') && (c3 == 'b' || c3 == 'B')) {
      return false;
    }
  }

  return true;
}

int ShowOpenBox(void)
{
  SFReply reply;
  Point pt = { 0, 0 };
  const int stdWidth = 348;

  pt.h = qd.screenBits.bounds.right / 2 - stdWidth / 2;

  SFGetFile(pt, NULL, RomFileFilter, -1, NULL, NULL, &reply);
  
  if(reply.good) {
    return LoadRom(reply.fName, reply.vRefNum);
  }
  
  return false;
}

void ShowAboutBox(void)
{
  DialogPtr dp;
  EventRecord e;
  DialogItemIndex hitItem;
  
  dp = GetNewDialog(DLOG_ABOUT, 0L, (WindowPtr) -1L);
  
  ModalDialog(NULL, &hitItem);

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
          AlertWrapper
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

static void HandleKeyEvent(int ch, int down)
{
  if (ch >= 'A' && ch <= 'Z') ch += 32; // tolower
  struct key_input *key = key_inputs;
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
        CautionAlertWrapper
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

    if (emulation_on && (now = TickCount()) != last_ticks) {
      last_ticks = now;
      // run one frame (70224 cycles = 154 scanlines * 456 cycles each
      unsigned long frame_end = cpu.cycle_count + 70224;
      while ((long) (frame_end - cpu.cycle_count) > 0) {
        dmg_step(&dmg);
      }
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

  return 0;
}
