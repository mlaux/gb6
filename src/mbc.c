#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mbc.h"
#include "dmg.h"
#include "rom.h"

static int is_mbc3(int type)
{
  return type >= 0x0f && type <= 0x13;
}

static int mbc1_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  if (addr >= 0 && addr <= 0x1fff) {
    int was_enabled = mbc->ram_enabled;
    mbc->ram_enabled = (data & 0xf) == 0xa;
    if (mbc->ram_enabled != was_enabled) {
      u8 *ram_base = mbc->ram_enabled ? &mbc->ram[0x2000 * mbc->ram_bank] : NULL;
      dmg_update_ram_bank(dmg, ram_base);
    }
    return 1;
  } else if (addr >= 0x2000 && addr <= 0x3fff) {
    mbc->rom_bank = data & 0x1f;
    int use_bank = mbc->rom_bank ? mbc->rom_bank : 1;
    dmg_update_rom_bank(dmg, use_bank);
    return 1;
  } else if (addr >= 0x4000 && addr <= 0x5fff) {
    mbc->ram_bank = data & 0x03;
    if (mbc->ram_enabled) {
      dmg_update_ram_bank(dmg, &mbc->ram[0x2000 * mbc->ram_bank]);
    }
    return 1;
  } else if (addr >= 0x6000 && addr <= 0x7fff) {
    //printf("sel %d\n", data);
    return 1;
  }
  return 0;
}

static void mbc3_latch_rtc(struct mbc *mbc)
{
  mbc->rtc_latched[0] = mbc->rtc_s;
  mbc->rtc_latched[1] = mbc->rtc_m;
  mbc->rtc_latched[2] = mbc->rtc_h;
  mbc->rtc_latched[3] = mbc->rtc_dl;
  mbc->rtc_latched[4] = mbc->rtc_dh;
}

static int mbc3_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  if (addr <= 0x1fff) {
    // RAM and Timer Enable
    int was_enabled = mbc->ram_enabled;
    mbc->ram_enabled = (data & 0xf) == 0xa;
    if (mbc->ram_enabled != was_enabled) {
      if (mbc->ram_enabled && mbc->rtc_select < 0) {
        dmg_update_ram_bank(dmg, &mbc->ram[0x2000 * mbc->ram_bank]);
      } else {
        dmg_update_ram_bank(dmg, NULL);
      }
    }
    return 1;
  }

  if (addr >= 0x2000 && addr <= 0x3fff) {
    // ROM Bank Number (7 bits)
    mbc->rom_bank = data & 0x7f;
    int use_bank = mbc->rom_bank ? mbc->rom_bank : 1;
    dmg_update_rom_bank(dmg, use_bank);
    return 1;
  }

  if (addr >= 0x4000 && addr <= 0x5fff) {
    // RAM Bank Number or RTC Register Select
    if (data <= 0x03) {
      // Select RAM bank
      mbc->ram_bank = data;
      mbc->rtc_select = -1;
      if (mbc->ram_enabled) {
        dmg_update_ram_bank(dmg, &mbc->ram[0x2000 * mbc->ram_bank]);
      }
    } else if (data >= 0x08 && data <= 0x0c) {
      // Select RTC register
      mbc->rtc_select = data;
      // RTC reads go through mbc_read, so set RAM to NULL
      dmg_update_ram_bank(dmg, NULL);
    }
    return 1;
  }

  if (addr >= 0x6000 && addr <= 0x7fff) {
    // Latch Clock Data
    if (mbc->rtc_latch_state == 0x00 && data == 0x01) {
      mbc3_latch_rtc(mbc);
    }
    mbc->rtc_latch_state = data;
    return 1;
  }

  return 0;
}

struct mbc *mbc_new(int type)
{
  static struct mbc mbc;

  // MBC1 types: 0x00-0x03
  // MBC3 types: 0x0f-0x13
  if (type > 3 && !is_mbc3(type)) {
    return NULL;
  }

  memset(&mbc, 0, sizeof mbc);
  mbc.type = type;
  mbc.rtc_select = -1;

  if (type == 3) {
    // MBC1+RAM+BATTERY
    mbc.has_battery = 1;
  } else if (is_mbc3(type)) {
    // 0x0f: MBC3+TIMER+BATTERY
    // 0x10: MBC3+TIMER+RAM+BATTERY
    // 0x11: MBC3
    // 0x12: MBC3+RAM
    // 0x13: MBC3+RAM+BATTERY
    mbc.has_rtc = (type == 0x0f || type == 0x10);
    mbc.has_battery = (type == 0x0f || type == 0x10 || type == 0x13);
  }

  return &mbc;
}

int mbc_write(struct mbc *mbc, struct dmg *dmg, u16 addr, u8 data)
{
  if (mbc->type == 0) {
    return 0;
  }
  if (is_mbc3(mbc->type)) {
    return mbc3_write(mbc, dmg, addr, data);
  }
  return mbc1_write(mbc, dmg, addr, data);
}

int mbc_ram_read(struct mbc *mbc, u16 addr, u8 *out)
{
  if (!mbc->ram_enabled) {
    return 0;
  }
  if (!is_mbc3(mbc->type) || mbc->rtc_select < 0) {
    return 0;
  }

  // RTC register read
  switch (mbc->rtc_select) {
    case 0x08: *out = mbc->rtc_latched[0]; return 1;
    case 0x09: *out = mbc->rtc_latched[1]; return 1;
    case 0x0a: *out = mbc->rtc_latched[2]; return 1;
    case 0x0b: *out = mbc->rtc_latched[3]; return 1;
    case 0x0c: *out = mbc->rtc_latched[4]; return 1;
  }
  return 0;
}

int mbc_ram_write(struct mbc *mbc, u16 addr, u8 data)
{
  if (!mbc->ram_enabled) {
    return 0;
  }
  if (!is_mbc3(mbc->type) || mbc->rtc_select < 0) {
    return 0;
  }

  // RTC register write
  switch (mbc->rtc_select) {
    case 0x08: mbc->rtc_s = data; return 1;
    case 0x09: mbc->rtc_m = data; return 1;
    case 0x0a: mbc->rtc_h = data; return 1;
    case 0x0b: mbc->rtc_dl = data; return 1;
    case 0x0c: mbc->rtc_dh = data; return 1;
  }
  return 0;
}

int mbc_save_ram(struct mbc *mbc, const char *filename)
{
  FILE *fp;

  if (!mbc->has_battery) {
    return 0;
  }

  fp = fopen(filename, "w");
  if (!fp) {
    return 0;
  }

  if (fwrite(mbc->ram, 1, RAM_SIZE, fp) < RAM_SIZE) {
    fclose(fp);
    return 0;
  }

  fclose(fp);
  return 1;
}

int mbc_load_ram(struct mbc *mbc, const char *filename)
{
  FILE *fp;

  if (!mbc->has_battery) {
    return 0;
  }

  fp = fopen(filename, "r");
  if (!fp) {
    return 0;
  }

  if (fread(mbc->ram, 1, RAM_SIZE, fp) < RAM_SIZE) {
    fclose(fp);
    return 0;
  }

  fclose(fp);
  return 1;
}