#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "rom.h"
#include "lcd.h"
#include "dmg.h"
#include "types.h"
#include "bootstrap.h"

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom, struct lcd *lcd)
{
    dmg->cpu = cpu;
    dmg->rom = rom;
    dmg->lcd = lcd;
}

u8 dmg_read(void *_dmg, u16 address)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    if (address < 0x100) {
        return dmg_boot_rom[address];
    } else if (address < 0x4000) {
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
    } else if (lcd_is_valid_addr(address)) {
        return lcd_read(dmg->lcd, address);
    } else if (address >= 0xff80 && address <= 0xfffe) {
        return dmg->zero_page[address - 0xff80];
    } else {
        // not sure about any of this yet
        // commented out bc of memory view window
        // fprintf(stderr, "don't know how to read 0x%04x\n", address);
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
    } else if (lcd_is_valid_addr(address)) {
        lcd_write(dmg->lcd, address, data);
    } else if (address >= 0xff80 && address <= 0xfffe) {
        dmg->zero_page[address - 0xff80] = data;
    } else {
        // not sure about any of this yet
    }
}

void dmg_step(void *_dmg)
{
    struct dmg *dmg = (struct dmg *) _dmg;

    // order of dependencies? i think cpu needs to step first then update
    // all other hw
    cpu_step(dmg->cpu);

    // each line takes 456 cycles
    if (dmg->cpu->cycle_count % 456 == 0) {
        int next_scanline = lcd_step(dmg->lcd);
        if (next_scanline == 144) {
            // vblank has started, draw all the stuff from ram into the lcd

            int lcdc = lcd_read(dmg->lcd, REG_LCDC);
            int bg_base = (lcdc & LCDC_BG_TILE_MAP) ? 0x9c00 : 0x9800;
            int window_base = (lcdc & LCDC_WINDOW_TILE_MAP) ? 0x9c00 : 0x9800;
            int use_unsigned = lcdc & LCDC_BG_TILE_DATA;
            int tilebase = use_unsigned ? 0x8000 : 0x9000;

            printf("base is %04x\n", bg_base);

            int k, off = 0;
            for (k = 0; k < 1024; k++) {
                int tile = dmg_read(dmg, bg_base + k);
                int eff_addr;
                if (use_unsigned) {
                    eff_addr = tilebase + 16 * tile;
                } else {
                    eff_addr = tilebase + 16 * (signed char) tile;
                }
                int b, i;
                for (b = 0; b < 16; b += 2) {
                    int data1 = dmg_read(dmg, eff_addr + b);
                    int data2 = dmg_read(dmg, eff_addr + b + 1);
                    for (i = 0; i < 8; i++) {
                        // monochrome for now
                        dmg->lcd->buf[off] |= (data1 & (1 << i)) ? 1 : 0;
                        dmg->lcd->buf[off] |= (data2 & (1 << i)) ? 1 : 0;
                        off++;
                    }
                }
            }

            // now copy 256x256 buf to 160x144 based on window registers
            lcd_copy(dmg->lcd);
            lcd_draw(dmg->lcd);
        }
    }
}