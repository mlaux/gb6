#ifndef _MBC_H
#define _MBC_H

#include "types.h"

#define RAM_SIZE 0x8000

struct dmg;

struct mbc {
  int type;
  int has_battery;
  int has_rtc;
  int rom_bank;
  int ram_bank;
  int ram_enabled;
  u8 ram[RAM_SIZE];

  // MBC3 RTC registers
  u8 rtc_s;       // seconds (0-59)
  u8 rtc_m;       // minutes (0-59)
  u8 rtc_h;       // hours (0-23)
  u8 rtc_dl;      // day counter low 8 bits
  u8 rtc_dh;      // day counter high bit + halt + carry flags
  u8 rtc_latched[5];  // latched values
  u8 rtc_latch_state; // for 0x00->0x01 latch sequence
  int rtc_select;     // which RTC register is mapped (0x08-0x0c), or -1 for RAM
};

struct mbc *mbc_new(int type);

// return 1 if handled, return 0 for base dmg behavior
int mbc_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data);
// read/write external RAM area (0xa000-0xbfff) - handles RTC registers for MBC3
int mbc_ram_read(struct mbc *mbc, u16 addr, u8 *out);
int mbc_ram_write(struct mbc *mbc, u16 addr, u8 data);
// 1 if success, 0 if error
int mbc_save_ram(struct mbc *mbc, const char *filename);
int mbc_load_ram(struct mbc *mbc, const char *filename);

#endif