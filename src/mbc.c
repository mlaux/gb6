#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "mbc.h"
#include "dmg.h"
#include "rom.h"

static int mbc1_read(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 *out_data)
{
  if (addr >= 0x4000 && addr <= 0x7fff) {
    int use_bank = mbc->rom_bank;
    if (!use_bank) {
      use_bank = 1;
    }
    *out_data = dmg->rom->data[0x4000 * use_bank + (addr & 0x3fff)];
    return 1;
  } else if (addr >= 0xa000 && addr <= 0xbfff) {
    if (mbc->ram_enabled) {
      *out_data = mbc->ram[0x2000 * mbc->ram_bank + (addr & 0x1fff)];
    } else {
      *out_data = 0xff;
    }
    return 1;
  }

  return 0;
}

static int mbc1_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  if (addr >= 0 && addr <= 0x1fff) {
    mbc->ram_enabled = (data & 0xf) == 0xa;
    return 1;
  } else if (addr >= 0x2000 && addr <= 0x3fff) {
    mbc->rom_bank = data & 0x1f;
    return 1;
  } else if (addr >= 0x4000 && addr <= 0x5fff) {
    mbc->ram_bank = data & 0x03;
    return 1;
  } else if (addr >= 0x6000 && addr <= 0x7fff) {
    //printf("sel %d\n", data);
    return 1;
  } else if (addr >= 0xa000 && addr <= 0xbfff) {
    if (mbc->ram_enabled) {
      mbc->ram[0x2000 * mbc->ram_bank + (addr & 0x1fff)] = data;
      return 1;
    }
  }
  return 0;
}

struct mbc *mbc_new(int type)
{
  static struct mbc mbc;
  if (type > 3) {
    return NULL;
  }
  mbc.type = type;

  return &mbc;
}

int mbc_read(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 *out_data)
{
  if (mbc->type == 0) {
    return 0;
  }
  return mbc1_read(mbc, dmg, addr, out_data);
}

int mbc_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  if (mbc->type == 0) {
    return 0;
  }
  return mbc1_write(mbc, dmg, addr, data);
}
