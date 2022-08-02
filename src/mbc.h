#ifndef _MBC_H
#define _MBC_H

#include "types.h"

struct dmg;

struct mbc {
  int type;
  int rom_bank;
  int ram_bank;
  int ram_enabled;
  u8 ram[0x8000];
};

struct mbc *mbc_new(int type);

// set *out_data and return 1 if handled, return 0 for base dmg behavior
int mbc_read(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 *out_data);
// return 1 if handled, return 0 for base dmg behavior
int mbc_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data);

#endif