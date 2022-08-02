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

    dmg->joypad = 0xf; // nothing pressed
    dmg->action_buttons = 0xf;
}

void dmg_set_button(struct dmg *dmg, int field, int button, int pressed)
{
    u8 *mod;
    if (field == FIELD_JOY) {
        mod = &dmg->joypad;
    } else if (field == FIELD_ACTION) {
        mod = &dmg->action_buttons;
    } else {
        printf("setting invalid button state\n");
        return;
    }

    if (pressed) {
        *mod &= ~button;
    } else {
        *mod |= button;
    }
}

static u8 get_button_state(struct dmg *dmg)
{
    u8 ret = 0;
    if (dmg->action_selected) {
        ret |= dmg->action_buttons;
    }
    if (dmg->joypad_selected) {
        ret |= dmg->joypad;
    }
    return ret;
}

static int counter;

u8 dmg_read(void *_dmg, u16 address)
{
    struct dmg *dmg = (struct dmg *) _dmg;
//    if (address < 0x100) {
//        return dmg_boot_rom[address];
//    } else if (address < 0x4000) {
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
    } else if (lcd_is_valid_addr(address)) {
        return lcd_read(dmg->lcd, address);
    } else if (address >= 0xff80 && address <= 0xfffe) {
        return dmg->zero_page[address - 0xff80];
    } else if (address == 0xff00) {
        return get_button_state(dmg);
    } else if (address == 0xff04) {
        counter++;
        return counter;
    } else if (address == 0xff0f) {
        return dmg->interrupt_requested;
    } else if (address == 0xffff) {
        return dmg->interrupt_enabled;
    } else {
        // not sure about any of this yet
        // commented out bc of memory view window
        // fprintf(stderr, "don't know how to read 0x%04x\n", address);
        return 0xff;
    }
}

void dmg_write(void *_dmg, u16 address, u8 data)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    if (address < 0x4000) {
        printf("warning: writing 0x%04x in rom\n", address);
    } else if (address < 0x8000) {
        // TODO switchable rom bank
        printf("warning: writing 0x%04x in rom\n", address);
    } else if (address < 0xa000) {
        dmg->video_ram[address - 0x8000] = data;
    } else if (address < 0xc000) {
        // TODO switchable ram bank
    } else if (address < 0xe000) {
        // printf("write ram %04x %02x\n", address, data);
        dmg->main_ram[address - 0xc000] = data;
    } else if (address == 0xFF46) {
        u16 src = data << 8;
        int k = 0;
        // printf("oam dma %04x\n", src);
        for (u16 addr = src; addr < src + 0xa0; addr++) {
            dmg->lcd->oam[k++] = dmg_read(dmg, addr);
        }
    } else if (lcd_is_valid_addr(address)) {
        lcd_write(dmg->lcd, address, data);
    } else if (address >= 0xff80 && address <= 0xfffe) {
        dmg->zero_page[address - 0xff80] = data;
    } else if (address == 0xff00) {
        dmg->joypad_selected = !(data & (1 << 4));
        dmg->action_selected = !(data & (1 << 5));
    } else if (address == 0xff0f) {
        dmg->interrupt_requested = data;
    } else if (address == 0xffff) {
        dmg->interrupt_enabled = data;
    } else {
        // not sure about any of this yet
    }
}

void dmg_request_interrupt(struct dmg *dmg, int nr)
{
    dmg->interrupt_requested |= nr;
}

// TODO move to lcd.c, it needs to be able to access dmg_read though
static void render_background(struct dmg *dmg, int lcdc)
{
    int bg_base = (lcdc & LCDC_BG_TILE_MAP) ? 0x9c00 : 0x9800;
    int window_base = (lcdc & LCDC_WINDOW_TILE_MAP) ? 0x9c00 : 0x9800;
    int use_unsigned = lcdc & LCDC_BG_TILE_DATA;
    int tilebase = use_unsigned ? 0x8000 : 0x9000;

    //printf("%04x %04x %04x\n", bg_base, window_base, tilebase);

    int k = 0, off = 0;
    int tile_y = 0, tile_x = 0;
    for (tile_y = 0; tile_y < 32; tile_y++) {
        for (tile_x = 0; tile_x < 32; tile_x++) {
            off = 256 * 8 * tile_y + 8 * tile_x;
            int tile = dmg_read(dmg, bg_base + (tile_y * 32 + tile_x));
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
                for (i = 7; i >= 0; i--) {
                    dmg->lcd->buf[off] = ((data1 & (1 << i)) ? 1 : 0) << 1;
                    dmg->lcd->buf[off] |= (data2 & (1 << i)) ? 1 : 0;
                    off++;
                }
                off += 248;
            }
        }
    }
}

struct oam_entry {
    u8 pos_y;
    u8 pos_x;
    u8 tile;
    u8 attrs;
};

// TODO: only ten per scanline, priority
static void render_objs(struct dmg *dmg)
{
    struct oam_entry *oam = (struct oam_entry *) dmg->lcd->oam;
    int k, lcd_x, lcd_y, off;
    for (k = 0; k < 40; k++, oam++) {
        if (oam->pos_y == 0 || oam->pos_y >= 160) {
            continue;
        }
        if (oam->pos_x == 0 || oam->pos_y >= 168) {
            continue;
        }

        lcd_x = oam->pos_x - 8;
        lcd_y = oam->pos_y - 16;

        off = 256 * lcd_y + lcd_x;
        int eff_addr = 0x8000 + 16 * oam->tile;
        int b, i;
        for (b = 0; b < 16; b += 2) {
            int data1 = dmg_read(dmg, eff_addr + b);
            int data2 = dmg_read(dmg, eff_addr + b + 1);
            for (i = 7; i >= 0; i--) {
                dmg->lcd->buf[off] = ((data1 & (1 << i)) ? 1 : 0) << 1;
                dmg->lcd->buf[off] |= (data2 & (1 << i)) ? 1 : 0;
                off++;
            }
            off += 248;
        }
    }
}

void dmg_step(void *_dmg)
{
    struct dmg *dmg = (struct dmg *) _dmg;

    // order of dependencies? i think cpu needs to step first then update
    // all other hw
    cpu_step(dmg->cpu);

    // each line takes 456 cycles

    int cycle_diff = dmg->cpu->cycle_count - dmg->last_lcd_update;

    if (cycle_diff >= 456) {
        dmg->last_lcd_update = dmg->cpu->cycle_count;
        int next_scanline = lcd_step(dmg->lcd);

        // update LYC
        if (next_scanline == lcd_read(dmg->lcd, REG_LYC)) {
            lcd_set_bit(dmg->lcd, REG_STAT, STAT_FLAG_MATCH);
            if (lcd_isset(dmg->lcd, REG_STAT, STAT_INTR_SOURCE_MATCH)) {
                dmg_request_interrupt(dmg, INT_LCDSTAT);
            }
        } else {
            lcd_clear_bit(dmg->lcd, REG_STAT, STAT_FLAG_MATCH);
        }

        if (next_scanline >= 144 && next_scanline < 154) {
            lcd_set_mode(dmg->lcd, 1);
        }

        // TODO: do all of this per-scanline instead of everything in vblank
        if (next_scanline == 144) {
            // vblank has started, draw all the stuff from ram into the lcd
            dmg_request_interrupt(dmg, INT_VBLANK);
            if (lcd_isset(dmg->lcd, REG_STAT, STAT_INTR_SOURCE_VBLANK)) {
                dmg_request_interrupt(dmg, INT_LCDSTAT);
            }

            int lcdc = lcd_read(dmg->lcd, REG_LCDC);
            if (lcdc & LCDC_ENABLE_BG) {
                render_background(dmg, lcdc);
            }

            if (lcdc & LCDC_ENABLE_WINDOW) {
                // printf("window\n");
            }

            if (lcdc & LCDC_ENABLE_OBJ) {
                render_objs(dmg);
            }

            // now copy 256x256 buf to 160x144 based on window registers
            lcd_copy(dmg->lcd);
            lcd_draw(dmg->lcd);
        }
    } else {
        int scan = lcd_read(dmg->lcd, REG_LY);
        if (scan < 144) {
            if (cycle_diff < 80) {
                lcd_set_mode(dmg->lcd, 2);
            } else if (cycle_diff < 230) {
                // just midpoint between 168 to 291, todo improve
                lcd_set_mode(dmg->lcd, 3);
            } else {
                lcd_set_mode(dmg->lcd, 0);
            }
        } else {
            // in vblank. mode should stay as 1
        }
    }
}