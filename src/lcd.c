#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "lcd.h"
#include "dmg.h"

void lcd_set_bit(struct lcd *lcd, u16 addr, u8 bit)
{
    lcd_write(lcd, addr, lcd_read(lcd, addr) | bit);
}

void lcd_clear_bit(struct lcd *lcd, u16 addr, u8 bit)
{
    lcd_write(lcd, addr, lcd_read(lcd, addr) & ~bit);
}

int lcd_isset(struct lcd *lcd, u16 addr, u8 bit)
{
    u8 val = lcd_read(lcd, addr);
    return val & bit;
}

void lcd_set_mode(struct lcd *lcd, int mode)
{
    u8 val = lcd_read(lcd, REG_STAT);
    lcd_write(lcd, REG_STAT, (val & 0xfc) | mode);
}

void lcd_new(struct lcd *lcd)
{
    const size_t size = 256 * 256;
    lcd->buf = malloc(size);
    lcd->window = malloc(size);
    memset(lcd->buf, 0, size);
    memset(lcd->window, 0, size);
    // todo < 8 bpp
    lcd->pixels = malloc(LCD_WIDTH * LCD_HEIGHT);
}

u8 lcd_is_valid_addr(u16 addr)
{
    return (addr >= 0xfe00 && addr < 0xfea0) || (addr >= REG_LCD_BASE && addr <= REG_LCD_LAST);
}

u8 lcd_read(struct lcd *lcd, u16 addr)
{
    if (addr >= 0xfe00 && addr < 0xfea0) {
        return lcd->oam[addr - 0xfe00];
    }
    return lcd->regs[addr - REG_LCD_BASE];
}

void lcd_write(struct lcd *lcd, u16 addr, u8 value)
{
    if (addr >= 0xfe00 && addr < 0xfea0) {
        lcd->oam[addr - 0xfe00] = value;
    } else {
        lcd->regs[addr - REG_LCD_BASE] = value;
    }
}

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        printf("warning: trying to write to (%d, %d) outside of lcd bounds\n", x, y);
        return;
    }
    lcd->pixels[y * LCD_WIDTH + x] = value;
}

void lcd_apply_scroll(struct lcd *lcd, int use_window)
{
    int scroll_y = lcd_read(lcd, REG_SCY);
    int scroll_x = lcd_read(lcd, REG_SCX);

    int win_x = lcd_read(lcd, REG_WX) - 7;
    int win_y = lcd_read(lcd, REG_WY);

    int lines;
    for (lines = 0; lines < 144; lines++) {
        int src_y = (scroll_y + lines) & 0xff;
        int cols;
        for (cols = 0; cols < 160; cols++) {
            int src_off = (src_y << 8) + ((scroll_x + cols) & 0xff);
            u8 *src = lcd->buf;
            if (use_window && cols >= win_x && lines >= win_y) {
                src_off = (((lines - win_y) & 0xff) << 8) + ((cols - win_x) & 0xff);
                src = lcd->window;
            }
            lcd->pixels[lines * 160 + cols] = src[src_off];
        }
    }
}

int lcd_step(struct lcd *lcd)
{
    // step to next scanline 0-153
    u8 next_scanline = (lcd_read(lcd, REG_LY) + 1) % 154;
    lcd_write(lcd, REG_LY, next_scanline);

    return next_scanline;
}

void lcd_render_background(struct dmg *dmg, int lcdc, int is_window)
{
    int bg_map = (lcdc & LCDC_BG_TILE_MAP) ? 0x1c00 : 0x1800;
    int window_map = (lcdc & LCDC_WINDOW_TILE_MAP) ? 0x1c00 : 0x1800;
    int map_off = is_window ? window_map : bg_map;
    int unsigned_mode = lcdc & LCDC_BG_TILE_DATA;
    int tile_base_off = unsigned_mode ? 0 : 0x1000;
    u8 palette = lcd_read(dmg->lcd, REG_BGP);
    u8 *vram = dmg->video_ram;
    u8 *dest = is_window ? dmg->lcd->window : dmg->lcd->buf;

    u8 pal[4] = {
        palette & 3,
        (palette >> 2) & 3,
        (palette >> 4) & 3,
        (palette >> 6) & 3
    };

    int tile_y, tile_x;
    for (tile_y = 0; tile_y < 32; tile_y++) {
        for (tile_x = 0; tile_x < 32; tile_x++) {
            u8 *p = dest + 256 * 8 * tile_y + 8 * tile_x;
            int tile_index = vram[map_off + tile_y * 32 + tile_x];
            int tile_off;

            if (unsigned_mode) {
                tile_off = tile_base_off + 16 * tile_index;
            } else {
                tile_off = tile_base_off + 16 * (signed char) tile_index;
            }

            int b;
            for (b = 0; b < 16; b += 2) {
                u8 data1 = vram[tile_off + b];
                u8 data2 = vram[tile_off + b + 1];
                int k;
                for (k = 0; k < 8; k++) {
                    int col_index = ((data1 >> 7) & 1) | (((data2 >> 7) & 1) << 1);
                    data1 <<= 1;
                    data2 <<= 1;
                    *p++ = pal[col_index];
                }
                p += 248;
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

// TODO: only ten per scanline, priority, attributes
void lcd_render_objs(struct dmg *dmg)
{
    struct oam_entry *oam = (struct oam_entry *) dmg->lcd->oam;
    int tall = lcd_isset(dmg->lcd, REG_LCDC, LCDC_OBJ_SIZE);
    u8 *vram = dmg->video_ram;
    u8 *pixels = dmg->lcd->pixels;

    int k;
    for (k = 0; k < 40; k++, oam++) {
        if (oam->pos_y == 0 || oam->pos_y >= 160) {
            continue;
        }
        if (oam->pos_x == 0 || oam->pos_x >= 168) {
            continue;
        }

        int tile_off = 16 * oam->tile;
        u8 palette = oam->attrs & OAM_ATTR_PALETTE
            ? lcd_read(dmg->lcd, REG_OBP1)
            : lcd_read(dmg->lcd, REG_OBP0);
        u8 pal[4] = {
            palette & 3,
            (palette >> 2) & 3,
            (palette >> 4) & 3,
            (palette >> 6) & 3
        };

        int lcd_x = oam->pos_x - 8;
        int lcd_y = oam->pos_y - 16;
        int tile_bytes = tall ? 32 : 16;
        int mirror_x = oam->attrs & OAM_ATTR_MIRROR_X;
        int mirror_y = oam->attrs & OAM_ATTR_MIRROR_Y;

        int b;
        for (b = 0; b < tile_bytes; b += 2) {
            int row_y = lcd_y + (b >> 1);
            if (row_y < 0 || row_y >= 144) {
                continue;
            }

            int use_b = mirror_y ? (tile_bytes - 2) - b : b;
            u8 data1 = vram[tile_off + use_b];
            u8 data2 = vram[tile_off + use_b + 1];

            // calculate visible x range for this row
            int x_start = lcd_x < 0 ? -lcd_x : 0;
            int x_end = lcd_x + 8 > 160 ? 160 - lcd_x : 8;

            u8 *p = pixels + row_y * 160 + lcd_x + x_start;
            int x;
            if (mirror_x) {
                // mirrored: read bits from LSB to MSB
                data1 >>= x_start;
                data2 >>= x_start;
                for (x = x_start; x < x_end; x++) {
                    int col_index = (data1 & 1) | ((data2 & 1) << 1);
                    data1 >>= 1;
                    data2 >>= 1;
                    if (col_index) {
                        *p = pal[col_index];
                    }
                    p++;
                }
            } else {
                // normal: read bits from MSB to LSB
                data1 <<= x_start;
                data2 <<= x_start;
                for (x = x_start; x < x_end; x++) {
                    int col_index = ((data1 >> 7) & 1) | (((data2 >> 7) & 1) << 1);
                    data1 <<= 1;
                    data2 <<= 1;
                    if (col_index) {
                        *p = pal[col_index];
                    }
                    p++;
                }
            }
        }
    }
}