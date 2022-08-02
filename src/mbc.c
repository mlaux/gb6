#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "mbc.h"
#include "dmg.h"
#include "rom.h"

static int mbc_noop_read(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 *out_data)
{
  return 0;
}

static int mbc_noop_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  return 0;
}

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

struct mbc mbc_noop = {
  mbc_noop_read,
  mbc_noop_write,
};

struct mbc mbc1 = {
  mbc1_read,
  mbc1_write,
};

struct mbc *mbc_new(int type)
{
  switch (type) {
    case 0:
      return &mbc_noop;
    case 1:
    case 2:
    case 3:
      return &mbc1;
  }
  return NULL;
}

int mbc_read(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 *out_data)
{
  return mbc->read_fn(mbc, dmg, addr, out_data);
}

int mbc_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  return mbc->write_fn(mbc, dmg, addr, data);
}
