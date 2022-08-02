#ifndef _MBC_H
#define _MBC_H

#include "types.h"

struct dmg;

struct mbc {
  int (*read_fn)(struct mbc *, struct dmg *, u16, u8 *);
  int (*write_fn)(struct mbc *, struct dmg *, u16, u8);
  int type;
  int rom_bank;
  int ram_bank;
  int ram_enabled;
  u8 ram[0x8000];
};

struct mbc *mbc_new(int type);
int mbc_read(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 *out_data);
int mbc_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data);

#endif