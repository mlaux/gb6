/* Game Boy emulator for 68k Macs
   emulator.h - declarations for emulator.c */
   
#ifndef EMULATOR_H
#define EMULATOR_H

#include "types.h"
#include "dmg.h"

#define WINDOW_TITLE "\pEmulator"

#define WINDOW_X 100
#define WINDOW_Y 100
#define WINDOW_WIDTH 256
#define WINDOW_HEIGHT 256

#define ALRT_NOT_ENOUGH_RAM 128
#define ALRT_4_LINE 129

#define DLOG_ABOUT 128
#define DLOG_STATE 129

#define MBAR_DEFAULT 128

#define MENU_APPLE 128
#define MENU_FILE 129
#define MENU_EMULATION 130

#define APPLE_ABOUT 1

#define FILE_OPEN 1
#define FILE_SCREENSHOT 2
#define FILE_QUIT 4

#define EMULATION_PAUSE 1
#define EMULATION_STATE 2
#define EMULATION_PREFERENCES 4
#define EMULATION_KEY_MAPPINGS 5

typedef unsigned char bool;
#define true 1
#define false 0

#endif