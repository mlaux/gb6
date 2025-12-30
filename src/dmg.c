#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "rom.h"
#include "lcd.h"
#include "dmg.h"
#include "mbc.h"
#include "types.h"
#include "bootstrap.h"
#include "platform.h"

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

u8 dmg_read_slow(struct dmg *dmg, u16 address)
{
    if (address == REG_LY) {
        // hack
        return lcd_step(dmg->lcd);
    }

    if (address == REG_STAT) {
        // also a hack
        u8 stat = lcd_read(dmg->lcd, REG_STAT);
        u8 new_stat = (stat & 0xfc) | (((stat & 3) + 1) & 3);
        lcd_write(dmg->lcd, REG_STAT, new_stat);
        return new_stat;
    }

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
extern void debug_log_string(const char *str);

// TODO remove, now handled by macros in cpu.c
u8 dmg_read(void *_dmg, u16 address)
{
    // char buf[128];
    u8 val;
    struct dmg *dmg = (struct dmg *) _dmg;
    u8 *page = dmg->read_page[address >> 8];
    if (page) {
        val = page[address & 0xff];
    } else {
        val = dmg_read_slow(dmg, address);
    }
    // sprintf(buf, "dmg_read pc=%04x addr=%04x value=%02x\n", dmg->cpu->pc, address, val);
    // debug_log_string(buf);
    return val;
}

void dmg_write_slow(struct dmg *dmg, u16 address, u8 data)
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

// sync hardware state - advance by given number of cycles
// this is per-scanline to allow for varying cycles, but in practice,
// cycles is always 70224, because the mac is too slow to check this
// after every scanline
void dmg_sync_hw(struct dmg *dmg, int cycles)
{
    // Timer DIV increments every cycle (wraps at 16 bits)
    dmg->timer_div += cycles;

    int scanlines = cycles / 456;
    int current_ly = lcd_read(dmg->lcd, REG_LY);
    int lyc = lcd_read(dmg->lcd, REG_LYC);

    // LYC match if lyc is in range [current_ly, current_ly + scanlines) mod 154
    int crosses_lyc = 0;
    if (scanlines >= 154) {
        // full frame always hits every scanline
        crosses_lyc = 1;
    } else {
        int end_ly = current_ly + scanlines;
        if (end_ly > 154) {
            // wraps around: check [current_ly, 154) and [0, end_ly % 154)
            crosses_lyc = (lyc >= current_ly) || (lyc < (end_ly % 154));
        } else {
            crosses_lyc = (lyc >= current_ly) && (lyc < end_ly);
        }
    }

    if (crosses_lyc) {
        lcd_set_bit(dmg->lcd, REG_STAT, STAT_FLAG_MATCH);
        if (lcd_isset(dmg->lcd, REG_STAT, STAT_INTR_SOURCE_MATCH)) {
            dmg_request_interrupt(dmg, INT_LCDSTAT);
        }
    } else {
        lcd_clear_bit(dmg->lcd, REG_STAT, STAT_FLAG_MATCH);
    }

    // Check if we cross scanline 144 (vblank)
    int crosses_vblank = 0;
    if (scanlines >= 154) {
        crosses_vblank = 1;
    } else {
        int end_ly = current_ly + scanlines;
        if (end_ly > 154) {
            crosses_vblank = (current_ly < 144) || ((end_ly % 154) >= 144);
        } else {
            crosses_vblank = (current_ly < 144) && (end_ly >= 144);
        }
    }

    if (crosses_vblank) {
        lcd_set_mode(dmg->lcd, 1);

        if (!(dmg->interrupt_requested & (1 << INT_VBLANK))) {
            dmg_request_interrupt(dmg, INT_VBLANK);
        }
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

    // this also advances every time it's read as a hack to get things unstuck...
    int new_ly = (current_ly + scanlines) % 154;
    lcd_write(dmg->lcd, REG_LY, new_ly);
}

void dmg_ei_di(void *_dmg, u16 enabled)
{
    struct dmg *dmg = (struct dmg *) _dmg;
    dmg->cpu->interrupt_enable = enabled ? 1 : 0;
}