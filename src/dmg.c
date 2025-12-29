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

    dmg_init_pages(dmg);
}

void dmg_init_pages(struct dmg *dmg)
{
    int k;

    // start with everything as slow path
    for (k = 0; k < 256; k++) {
        dmg->read_page[k] = NULL;
        dmg->write_page[k] = NULL;
    }

    // ROM bank 0: 0x0000-0x3fff (pages 0x00-0x3f)
    for (k = 0x00; k <= 0x3f; k++) {
        dmg->read_page[k] = &dmg->rom->data[k << 8];
    }

    // ROM bank 1: 0x4000-0x7fff (pages 0x40-0x7f)
    // MBC will update this when switching banks
    for (k = 0x40; k <= 0x7f; k++) {
        dmg->read_page[k] = &dmg->rom->data[k << 8];
    }

    // video RAM: 0x8000-0x9fff (pages 0x80-0x9f)
    for (k = 0x80; k <= 0x9f; k++) {
        int offset = (k - 0x80) << 8;
        dmg->read_page[k] = &dmg->video_ram[offset];
        dmg->write_page[k] = &dmg->video_ram[offset];
    }

    // external RAM: 0xa000-0xbfff (pages 0xa0-0xbf)
    // leave NULL - MBC handles this

    // work RAM: 0xc000-0xdfff (pages 0xc0-0xdf)
    for (k = 0xc0; k <= 0xdf; k++) {
        int offset = (k - 0xc0) << 8;
        dmg->read_page[k] = &dmg->main_ram[offset];
        dmg->write_page[k] = &dmg->main_ram[offset];
    }

    // echo RAM: 0xe000-0xfdff (pages 0xe0-0xfd)
    for (k = 0xe0; k <= 0xfd; k++) {
        int offset = (k - 0xe0) << 8;
        dmg->read_page[k] = &dmg->main_ram[offset];
        dmg->write_page[k] = &dmg->main_ram[offset];
    }

    // pages 0xfe and 0xff stay NULL for special handling
}

void dmg_update_rom_bank(struct dmg *dmg, int bank)
{
    int k;
    u8 *bank_base = &dmg->rom->data[bank * 0x4000];
    for (k = 0x40; k <= 0x7f; k++) {
        dmg->read_page[k] = &bank_base[(k - 0x40) << 8];
    }
}

void dmg_update_ram_bank(struct dmg *dmg, u8 *ram_base)
{
    int k;
    for (k = 0xa0; k <= 0xbf; k++) {
        if (ram_base) {
            int offset = (k - 0xa0) << 8;
            dmg->read_page[k] = &ram_base[offset];
            dmg->write_page[k] = &ram_base[offset];
        } else {
            dmg->read_page[k] = NULL;
            dmg->write_page[k] = NULL;
        }
    }
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

static u8 dmg_read_slow(struct dmg *dmg, u16 address)
{
    // OAM and LCD registers
    if (lcd_is_valid_addr(address)) {
        return lcd_read(dmg->lcd, address);
    }

    // high RAM
    if (address >= 0xff80 && address <= 0xfffe) {
        return dmg->zero_page[address - 0xff80];
    }

    // I/O registers
    if (address == 0xff00) {
        return get_button_state(dmg);
    }
    if (address == REG_TIMER_DIV) {
        return (dmg->timer_div & 0xff00) >> 8;
    }
    if (address == REG_TIMER_COUNT) {
        return dmg->timer_count;
    }
    if (address == REG_TIMER_MOD) {
        return dmg->timer_mod;
    }
    if (address == REG_TIMER_CONTROL) {
        return dmg->timer_control;
    }
    if (address >= 0xff10 && address <= 0xff3f) {
        return dmg->audio_regs[address - 0xff10];
    }
    if (address == 0xff0f) {
        return dmg->interrupt_requested;
    }
    if (address == 0xffff) {
        return dmg->interrupt_enabled;
    }

    // external RAM not enabled, or RTC register selected
    if (address >= 0xa000 && address < 0xc000) {
        u8 val;
        if (mbc_ram_read(dmg->rom->mbc, address, &val)) {
            return val;
        }
        return 0xff;
    }

    return 0xff;
}

u8 dmg_read(void *_dmg, u16 address)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    u8 *page = dmg->read_page[address >> 8];
    if (page) {
        return page[address & 0xff];
    }
    return dmg_read_slow(dmg, address);
}

static void dmg_write_slow(struct dmg *dmg, u16 address, u8 data)
{
    // ROM region writes go to MBC for bank switching
    if (address < 0x8000) {
        mbc_write(dmg->rom->mbc, dmg, address, data);
        return;
    }

    // external RAM not enabled, or RTC register selected
    if (address >= 0xa000 && address < 0xc000) {
        mbc_ram_write(dmg->rom->mbc, address, data);
        return;
    }

    // OAM DMA
    if (address == 0xff46) {
        u16 src = data << 8;
        int k = 0;
        for (u16 addr = src; addr < src + 0xa0; addr++) {
            dmg->lcd->oam[k++] = dmg_read(dmg, addr);
        }
        return;
    }

    // OAM and LCD registers
    if (lcd_is_valid_addr(address)) {
        lcd_write(dmg->lcd, address, data);
        return;
    }

    // high RAM
    if (address >= 0xff80 && address <= 0xfffe) {
        dmg->zero_page[address - 0xff80] = data;
        return;
    }

    // I/O registers
    if (address == 0xff00) {
        dmg->joypad_selected = !(data & (1 << 4));
        dmg->action_selected = !(data & (1 << 5));
        return;
    }
    if (address == REG_TIMER_DIV) {
        dmg->timer_div = 0;
        return;
    }
    if (address == REG_TIMER_COUNT) {
        dmg->timer_count = data;
        return;
    }
    if (address == REG_TIMER_MOD) {
        dmg->timer_mod = data;
        return;
    }
    if (address == REG_TIMER_CONTROL) {
        dmg->timer_control = data;
        return;
    }
    if (address >= 0xff10 && address <= 0xff3f) {
        dmg->audio_regs[address - 0xff10] = data;
        return;
    }
    if (address == 0xff0f) {
        dmg->interrupt_requested = data;
        return;
    }
    if (address == 0xffff) {
        dmg->interrupt_enabled = data;
        return;
    }
}

void dmg_write(void *_dmg, u16 address, u8 data)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    u8 *page = dmg->write_page[address >> 8];
    if (page) {
        page[address & 0xff] = data;
        return;
    }
    dmg_write_slow(dmg, address, data);
}

void dmg_request_interrupt(struct dmg *dmg, int nr)
{
    dmg->interrupt_requested |= nr;
}

static void timer_step(struct dmg *dmg)
{
    dmg->timer_div += 4;
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

    cpu_step(dmg->cpu);
    //timer_step(dmg);

    dmg->timer_div += 4;

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

        // TODO: do all of this per-scanline instead of everything in vblank
        if (next_scanline == 144) {
            lcd_set_mode(dmg->lcd, 1);

            // vblank has started, draw all the stuff from ram into the lcd
            dmg_request_interrupt(dmg, INT_VBLANK);
            if (lcd_isset(dmg->lcd, REG_STAT, STAT_INTR_SOURCE_VBLANK)) {
                dmg_request_interrupt(dmg, INT_LCDSTAT);
            }

            if (dmg->frames_rendered % dmg->frame_skip == 0) {
                int lcdc = lcd_read(dmg->lcd, REG_LCDC);
                if (lcdc & LCDC_ENABLE_BG) {
                    int window_enabled = lcdc & LCDC_ENABLE_WINDOW;
                    lcd_render_background(dmg, lcdc, window_enabled);
                }

                if (lcdc & LCDC_ENABLE_OBJ) {
                    lcd_render_objs(dmg);
                }

                lcd_draw(dmg->lcd);
            }


            dmg->frames_rendered++;
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