#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "lcd.h"
#include "dmg.h"

// LUT for decoding 4 pixels at once from tile data
// Index = (data1_nibble << 4) | data2_nibble
// Output = 4 palette-mapped pixel values (ready to write to framebuffer)
static u8 tile_decode_lut[256][4];

// Base LUT with color indices 0-3 (palette-independent)
static u8 tile_decode_base[256][4];

void lcd_init_lut(void)
{
    int n1, n2;
    for (n1 = 0; n1 < 16; n1++) {
        for (n2 = 0; n2 < 16; n2++) {
            int idx = (n1 << 4) | n2;
            // Bit 3 -> pixel 0, bit 2 -> pixel 1, etc. (MSB first)
            tile_decode_base[idx][0] = ((n1 >> 3) & 1) | (((n2 >> 3) & 1) << 1);
            tile_decode_base[idx][1] = ((n1 >> 2) & 1) | (((n2 >> 2) & 1) << 1);
            tile_decode_base[idx][2] = ((n1 >> 1) & 1) | (((n2 >> 1) & 1) << 1);
            tile_decode_base[idx][3] = (n1 & 1) | ((n2 & 1) << 1);
        }
    }
    // Initialize with identity palette (no mapping)
    lcd_update_palette_lut(0xe4); // default: 3,2,1,0
}

void lcd_update_palette_lut(u8 palette)
{
    u8 pal[4];
    int k;

    pal[0] = palette & 3;
    pal[1] = (palette >> 2) & 3;
    pal[2] = (palette >> 4) & 3;
    pal[3] = (palette >> 6) & 3;

    for (k = 0; k < 256; k++) {
        tile_decode_lut[k][0] = pal[tile_decode_base[k][0]];
        tile_decode_lut[k][1] = pal[tile_decode_base[k][1]];
        tile_decode_lut[k][2] = pal[tile_decode_base[k][2]];
        tile_decode_lut[k][3] = pal[tile_decode_base[k][3]];
    }
}

struct oam_entry {
    u8 pos_y;
    u8 pos_x;
    u8 tile;
    u8 attrs;
};

void lcd_new(struct lcd *lcd)
{
    const size_t size = 256 * 256;
    // todo < 8 bpp
    lcd->pixels = malloc(LCD_WIDTH * LCD_HEIGHT);
}

u8 lcd_is_valid_addr(u16 addr)
{
    return (addr >= 0xfe00 && addr < 0xfea0) || (addr >= REG_LCD_BASE && addr <= REG_LCD_LAST);
}

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        printf("warning: trying to write to (%d, %d) outside of lcd bounds\n", x, y);
        return;
    }
    lcd->pixels[y * LCD_WIDTH + x] = value;
}

int lcd_step(struct lcd *lcd)
{
    // step to next scanline 0-153
    u8 next_scanline = (lcd_read(lcd, REG_LY) + 1) % 154;
    lcd_write(lcd, REG_LY, next_scanline);

    return next_scanline;
}

// render a row of pixels from a tile, handling partial tiles
// data1/data2 should be pre-shifted so the first pixel we want is in bit 7
static void render_tile_row(
    u8 *p,
    u8 data1,
    u8 data2,
    int count
) {
    u8 *dec;
    int idx_hi = (data1 & 0xf0) | (data2 >> 4);
    int idx_lo = ((data1 & 0x0f) << 4) | (data2 & 0x0f);

    dec = tile_decode_lut[idx_hi];
    if (count >= 4) {
        p[0] = dec[0];
        p[1] = dec[1];
        p[2] = dec[2];
        p[3] = dec[3];

        dec = tile_decode_lut[idx_lo];
        if (count >= 8) {
            p[4] = dec[0];
            p[5] = dec[1];
            p[6] = dec[2];
            p[7] = dec[3];
        } else {
            int k;
            for (k = 4; k < count; k++) {
                p[k] = dec[k - 4];
            }
        }
    } else {
        int k;
        for (k = 0; k < count; k++) {
            p[k] = dec[k];
        }
    }
}

void lcd_render_background(struct dmg *dmg, int lcdc, int window_enabled)
{
    u8 *vram = dmg->video_ram;
    u8 *pixels = dmg->lcd->pixels;

    int scx = lcd_read(dmg->lcd, REG_SCX);
    int scy = lcd_read(dmg->lcd, REG_SCY);
    int wx = lcd_read(dmg->lcd, REG_WX) - 7;
    int wy = lcd_read(dmg->lcd, REG_WY);

    int bg_map_off = (lcdc & LCDC_BG_TILE_MAP) ? 0x1c00 : 0x1800;
    int win_map_off = (lcdc & LCDC_WINDOW_TILE_MAP) ? 0x1c00 : 0x1800;
    int unsigned_mode = lcdc & LCDC_BG_TILE_DATA;
    int tile_base_off = unsigned_mode ? 0 : 0x1000;

    // palette is baked into tile_decode_lut, updated on BGP writes

    int sy;
    for (sy = 0; sy < 144; sy++) {
        u8 *p = pixels + sy * 160;
        int window_active = window_enabled && sy >= wy && wx < 160;
        int bg_end_x = window_active ? (wx > 0 ? wx : 0) : 160;

        // render background portion
        if (bg_end_x > 0) {
            int bg_y = (sy + scy) & 0xff;
            int tile_row = bg_y >> 3;
            int row_in_tile = bg_y & 7;
            int bg_x = scx;
            int sx = 0;

            while (sx < bg_end_x) {
                int tile_col = (bg_x >> 3) & 31;
                int start_bit = bg_x & 7;
                int pixels_in_tile = 8 - start_bit;
                if (sx + pixels_in_tile > bg_end_x) {
                    pixels_in_tile = bg_end_x - sx;
                }

                int tile_idx = vram[bg_map_off + tile_row * 32 + tile_col];
                int tile_off;
                if (unsigned_mode) {
                    tile_off = tile_base_off + 16 * tile_idx;
                } else {
                    tile_off = tile_base_off + 16 * (signed char) tile_idx;
                }

                u8 data1 = vram[tile_off + row_in_tile * 2];
                u8 data2 = vram[tile_off + row_in_tile * 2 + 1];
                data1 <<= start_bit;
                data2 <<= start_bit;

                render_tile_row(p, data1, data2, pixels_in_tile);
                p += pixels_in_tile;
                sx += pixels_in_tile;
                bg_x = (bg_x + pixels_in_tile) & 0xff;
            }
        }

        // render window portion
        if (window_active) {
            int win_y = sy - wy;
            int tile_row = win_y >> 3;
            int row_in_tile = win_y & 7;
            int win_x = 0;
            int sx = wx > 0 ? wx : 0;

            // if window starts off-screen left, skip those pixels
            if (wx < 0) {
                win_x = -wx;
            }

            while (sx < 160) {
                int tile_col = (win_x >> 3) & 31;
                int start_bit = win_x & 7;
                int pixels_in_tile = 8 - start_bit;
                if (sx + pixels_in_tile > 160) {
                    pixels_in_tile = 160 - sx;
                }

                int tile_idx = vram[win_map_off + tile_row * 32 + tile_col];
                int tile_off;
                if (unsigned_mode) {
                    tile_off = tile_base_off + 16 * tile_idx;
                } else {
                    tile_off = tile_base_off + 16 * (signed char) tile_idx;
                }

                u8 data1 = vram[tile_off + row_in_tile * 2];
                u8 data2 = vram[tile_off + row_in_tile * 2 + 1];
                data1 <<= start_bit;
                data2 <<= start_bit;

                render_tile_row(p, data1, data2, pixels_in_tile);
                p += pixels_in_tile;
                sx += pixels_in_tile;
                win_x += pixels_in_tile;
            }
        }
    }
}

// TODO: only ten per scanline, priority, attributes
void lcd_render_objs(struct dmg *dmg)
{
    struct oam_entry *oam = &((struct oam_entry *) dmg->lcd->oam)[39];
    int tall = lcd_isset(dmg->lcd, REG_LCDC, LCDC_OBJ_SIZE);
    u8 *vram = dmg->video_ram;
    u8 *pixels = dmg->lcd->pixels;

    int k;
    for (k = 39; k >= 0; k--, oam--) {
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