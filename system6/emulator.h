/* Game Boy emulator for 68k Macs
   emulator.h - declarations for emulator.c */
   
#ifndef EMULATOR_H
#define EMULATOR_H

#include "types.h"
#include "dmg.h"

#define WINDOW_TITLE "\pgb6"

#define WINDOW_X 3
#define WINDOW_Y 40
#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 299

#define ALRT_NOT_ENOUGH_RAM 128
#define ALRT_4_LINE 129

#define DLOG_ABOUT 128

#define MBAR_DEFAULT 128

#define MENU_APPLE 128
#define MENU_FILE 129
#define MENU_EDIT 130

#define APPLE_ABOUT 1

#define FILE_OPEN 1
#define FILE_SCREENSHOT 2
#define FILE_QUIT 4

#define EDIT_PREFERENCES 1
#define EDIT_KEY_MAPPINGS 2

int LoadRom(Str63, short);

void set_status_bar(const char *str);

#endif