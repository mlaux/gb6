#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "rom.h"
#include "lcd.h"
#include "dmg.h"
#include "mbc.h"
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
    u8 ret = 0xf0;
    if (dmg->action_selected) {
        ret |= dmg->action_buttons;
    }
    if (dmg->joypad_selected) {
        ret |= dmg->joypad;
    }
    return ret;
}

u8 dmg_read(void *_dmg, u16 address)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    u8 mbc_data;

    if (mbc_read(dmg->rom->mbc, dmg, address, &mbc_data)) {
        return mbc_data;
    }

    if (address < 0x4000) {
        return dmg->rom->data[address];
    } else if (address < 0x8000) {
        return dmg->rom->data[address];
    } else if (address < 0xa000) {
        return dmg->video_ram[address - 0x8000];
    } else if (address < 0xc000) {
        printf("RAM bank not handled by MBC\n");
        return 0xff;
    } else if (address < 0xe000) {
        return dmg->main_ram[address - 0xc000];
    } else if (lcd_is_valid_addr(address)) {
        return lcd_read(dmg->lcd, address);
    } else if (address >= 0xff80 && address <= 0xfffe) {
        return dmg->zero_page[address - 0xff80];
    } else if (address == 0xff00) {
        return get_button_state(dmg);
    } else if (address == REG_TIMER_DIV) {
        return (dmg->timer_div & 0xff00) >> 8;
    } else if (address == REG_TIMER_COUNT) {
        return dmg->timer_count;
    } else if (address == REG_TIMER_MOD) {
        return dmg->timer_mod;
    } else if (address == REG_TIMER_CONTROL) {
        return dmg->timer_control;
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

    if (mbc_write(dmg->rom->mbc, dmg, address, data)) {
        return;
    }

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
    } else if (address == REG_TIMER_DIV) {
        dmg->timer_div = 0;
    } else if (address == REG_TIMER_COUNT) {
        printf("write timer count\n");
        dmg->timer_count = data;
    } else if (address == REG_TIMER_MOD) {
        printf("write timer mod\n");
        dmg->timer_mod = data;
    } else if (address == REG_TIMER_CONTROL) {
        printf("write timer control\n");
        dmg->timer_control = data;
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

static void timer_step(struct dmg *dmg)
{
    dmg->timer_div++;
    return;

    if (!(dmg_read(dmg, REG_TIMER_CONTROL) & TIMER_CONTROL_ENABLED)) {
        return;
    }

    int passed = dmg->cpu->cycle_count - dmg->last_timer_update;

    u8 counter = dmg_read(dmg, REG_TIMER_COUNT);
    u8 modulo = dmg_read(dmg, REG_TIMER_MOD);

    counter++;
    if (!counter) {
        counter = modulo;
        dmg_request_interrupt(dmg, INT_TIMER);
    }

    dmg_write(dmg, REG_TIMER_COUNT, counter);
    dmg->last_timer_update = dmg->cpu->cycle_count;
}

void dmg_step(void *_dmg)
{
    struct dmg *dmg = (struct dmg *) _dmg;

    // order of dependencies? i think cpu needs to step first then update
    // all other hw
    cpu_step(dmg->cpu);
    timer_step(dmg);

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
                int window_enabled = lcdc & LCDC_ENABLE_WINDOW;
                lcd_render_background_scrolled(dmg, lcdc, window_enabled);
            }

            if (lcdc & LCDC_ENABLE_OBJ) {
                lcd_render_objs(dmg);
            }

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