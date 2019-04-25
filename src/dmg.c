#include <stdio.h>

#include "cpu.h"
#include "rom.h"
#include "dmg.h"
#include "types.h"
#include "bootstrap.h"

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom)
{
    dmg->cpu = cpu;
    dmg->rom = rom;
}

u8 dmg_read(void *_dmg, u16 address)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    if (address < 0x100) {
        return dmg_boot_rom[address];
    }
    if (address < 0x4000) {
        return dmg->rom->data[address];
    } else if (address < 0x8000) {
        // TODO switchable rom bank
        return dmg->rom->data[address];
    } else if (address < 0xa000) {
        return dmg->video_ram[address - 0x8000];
    } else if (address < 0xc000) {
        // TODO switchable ram bank
        return 0;
    } else if (address < 0xe000) {
        return dmg->main_ram[address - 0xc000];
    } else if (address >= 0xff80 && address <= 0xfffe) {
        return dmg->zero_page[address - 0xff80];
    } else {
        // not sure about any of this yet
        return 0;
    }
}

void dmg_write(void *_dmg, u16 address, u8 data)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    if (address < 0x4000) {
        printf("warning: writing 0x%04x in rom\n", address);
        dmg->rom->data[address] = data;
    } else if (address < 0x8000) {
        // TODO switchable rom bank
        printf("warning: writing 0x%04x in rom\n", address);
        dmg->rom->data[address] = data;
    } else if (address < 0xa000) {
        dmg->video_ram[address - 0x8000] = data;
    } else if (address < 0xc000) {
        // TODO switchable ram bank
    } else if (address < 0xe000) {
        dmg->main_ram[address - 0xc000] = data;
    } else if (address >= 0xff80 && address <= 0xfffe) {
        dmg->zero_page[address - 0xff80] = data;
    } else {
        // not sure about any of this yet
    }
}
