#include "cpu.h"
#include "rom.h"
#include "dmg.h"
#include "types.h"

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom)
{
    dmg->cpu = cpu;
    dmg->rom = rom;
}

u8 dmg_read(void *_dmg, u16 address)
{
    struct dmg *dmg = (struct dmg *) _dmg;
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
    } else {
        // not sure about any of this yet
        return 0;
    }
}

void dmg_write(void *_dmg, u16 address, u8 data)
{
    struct dmg *dmg = (struct dmg *) _dmg;
}
