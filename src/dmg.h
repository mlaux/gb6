#ifndef _DMG_H
#define _DMG_H

#include "cpu.h"
#include "rom.h"
#include "lcd.h"

struct dmg {
    struct cpu *cpu;
    struct rom *rom;
    struct lcd *lcd;
    u8 main_ram[0x2000];
    u8 video_ram[0x2000];
    u8 zero_page[0x80];
};

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom, struct lcd *lcd);

// why did i make these void *
u8 dmg_read(void *dmg, u16 address);
void dmg_write(void *dmg, u16 address, u8 data);

void dmg_step(void *dmg);

#endif
