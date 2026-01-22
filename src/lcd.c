#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "lcd.h"
#include "dmg.h"

// LUT for decoding 4 pixels at once from tile data
// Index = (data1_nibble << 4) | data2_nibble
// Output = 4 palette-mapped pixel values packed into one byte (p0<<6|p1<<4|p2<<2|p3)
static u8 tile_decode_packed[256];

// Base LUT with color indices 0-3 (palette-independent)
static u8 tile_decode_base[256][4];

// Packed pixel buffer: 4 pixels per byte, 40 bytes per row
static u8 pixels[40 * 144];

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
        u8 p0 = pal[tile_decode_base[k][0]];
        u8 p1 = pal[tile_decode_base[k][1]];
        u8 p2 = pal[tile_decode_base[k][2]];
        u8 p3 = pal[tile_decode_base[k][3]];
        tile_decode_packed[k] = (p0 << 6) | (p1 << 4) | (p2 << 2) | p3;
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
    // todo < 8 bpp
    lcd->pixels = pixels;
}

u8 lcd_is_valid_addr(u16 addr)
{
    return (addr >= 0xfe00 && addr < 0xfea0) || (addr >= REG_LCD_BASE && addr <= REG_LCD_LAST);
}

int lcd_step(struct lcd *lcd)
{
    // step to next scanline 0-153
    u8 next_scanline = (lcd_read(lcd, REG_LY) + 1) % 154;
    lcd_write(lcd, REG_LY, next_scanline);

    return next_scanline;
}

// render 8 aligned pixels (full tile row) to packed output
// writes 2 bytes: pixels 0-3 and pixels 4-7
static inline void render_tile_row_packed(u8 *p, u8 data1, u8 data2)
{
    int idx_hi = (data1 & 0xf0) | (data2 >> 4);
    int idx_lo = ((data1 & 0x0f) << 4) | (data2 & 0x0f);
    p[0] = tile_decode_packed[idx_hi];
    p[1] = tile_decode_packed[idx_lo];
}

// helper to extract single pixel from packed byte (pixel 0 is bits 7-6)
static inline u8 packed_get_pixel(u8 packed, int pixel_idx)
{
    return (packed >> (6 - pixel_idx * 2)) & 3;
}

// helper to set single pixel in packed byte
static inline u8 packed_set_pixel(u8 packed, int pixel_idx, u8 value)
{
    int shift = 6 - pixel_idx * 2;
    return (packed & ~(3 << shift)) | ((value & 3) << shift);
}

// render partial pixels at start of a tile row (handles SCX misalignment)
// pixel_offset: which pixel in the tile to start from (1-7)
// count: how many pixels to render (1-7)
// byte_idx: which pixel position in the output byte to start writing (0-3)
static inline void render_partial_start(
    u8 *p,
    u8 data1,
    u8 data2,
    int pixel_offset,
    int count,
    int byte_idx
) {
    int idx_hi = (data1 & 0xf0) | (data2 >> 4);
    int idx_lo = ((data1 & 0x0f) << 4) | (data2 & 0x0f);
    u8 packed_hi = tile_decode_packed[idx_hi];
    u8 packed_lo = tile_decode_packed[idx_lo];
    int k;

    for (k = 0; k < count; k++) {
        int src_pixel = pixel_offset + k;
        u8 value = (src_pixel < 4)
            ? packed_get_pixel(packed_hi, src_pixel)
            : packed_get_pixel(packed_lo, src_pixel - 4);
        int dst_pixel = byte_idx + k;
        int dst_byte = dst_pixel >> 2;
        int dst_bit = dst_pixel & 3;
        p[dst_byte] = packed_set_pixel(p[dst_byte], dst_bit, value);
    }
}

void lcd_render_background(struct dmg *dmg, int lcdc, int window_enabled)
{
    u8 *vram = dmg->video_ram;
    u8 *out = dmg->lcd->pixels;

    int scx = lcd_read(dmg->lcd, REG_SCX);
    int scy = lcd_read(dmg->lcd, REG_SCY);
    int wx = lcd_read(dmg->lcd, REG_WX) - 7;
    int wy = lcd_read(dmg->lcd, REG_WY);

    int bg_map_off = (lcdc & LCDC_BG_TILE_MAP) ? 0x1c00 : 0x1800;
    int win_map_off = (lcdc & LCDC_WINDOW_TILE_MAP) ? 0x1c00 : 0x1800;
    int unsigned_mode = lcdc & LCDC_BG_TILE_DATA;
    int tile_base_off = unsigned_mode ? 0 : 0x1000;

    int sy;
    for (sy = 0; sy < 144; sy++) {
        u8 *row = out + sy * 40;
        int window_active = window_enabled && sy >= wy && wx < 160;
        int bg_end_x = window_active ? (wx > 0 ? wx : 0) : 160;

        // render background portion
        if (bg_end_x > 0) {
            int bg_y = (sy + scy) & 0xff;
            int tile_row = bg_y >> 3;
            int row_in_tile = bg_y & 7;
            int start_bit = scx & 7;
            int sx = 0;
            int bg_x = scx;

            // fast path: when scx is 4-pixel aligned, output stays byte-aligned
            if ((scx & 3) == 0) {
                // handle partial first tile if SCX not 8-aligned (but is 4-aligned)
                if (start_bit != 0) {
                    int tile_col = (bg_x >> 3) & 31;
                    int pixels_first = 8 - start_bit; // will be 4 since scx % 4 == 0

                    int tile_idx = vram[bg_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    // write just the low nibble (pixels 4-7) to first output byte
                    int idx_lo = ((data1 & 0x0f) << 4) | (data2 & 0x0f);
                    row[0] = tile_decode_packed[idx_lo];
                    sx = 4;
                    bg_x = (scx + 4) & 0xff;
                }

                // render full aligned tiles (8 pixels = 2 packed bytes each)
                while (sx + 8 <= bg_end_x) {
                    int tile_col = (bg_x >> 3) & 31;
                    int tile_idx = vram[bg_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    render_tile_row_packed(row + (sx >> 2), data1, data2);
                    sx += 8;
                    bg_x = (bg_x + 8) & 0xff;
                }

                // handle partial last tile before window
                if (sx < bg_end_x) {
                    int tile_col = (bg_x >> 3) & 31;
                    int pixels_last = bg_end_x - sx;
                    int tile_idx = vram[bg_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    // pixels_last will be 4 (one nibble) since we're 4-aligned
                    int idx_hi = (data1 & 0xf0) | (data2 >> 4);
                    row[sx >> 2] = tile_decode_packed[idx_hi];
                }
            } else {
                // slow path: scx not 4-aligned, use per-pixel rendering
                while (sx < bg_end_x) {
                    int tile_col = (bg_x >> 3) & 31;
                    int pixel_in_tile = bg_x & 7;
                    int pixels_in_tile = 8 - pixel_in_tile;
                    if (sx + pixels_in_tile > bg_end_x) {
                        pixels_in_tile = bg_end_x - sx;
                    }

                    int tile_idx = vram[bg_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    render_partial_start(row, data1, data2, pixel_in_tile, pixels_in_tile, sx);
                    sx += pixels_in_tile;
                    bg_x = (bg_x + pixels_in_tile) & 0xff;
                }
            }
        }

        // render window portion
        if (window_active) {
            int win_y = sy - wy;
            int tile_row = win_y >> 3;
            int row_in_tile = win_y & 7;
            int sx = wx > 0 ? wx : 0;
            int win_x = wx < 0 ? -wx : 0;

            // check if output position is 4-pixel aligned for fast path
            if ((sx & 3) == 0 && (win_x & 3) == 0) {
                // fast path: both screen position and window offset are 4-aligned
                int start_bit = win_x & 7;
                if (start_bit != 0) {
                    // win_x is 4-aligned but not 8-aligned
                    int tile_col = (win_x >> 3) & 31;
                    int tile_idx = vram[win_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    int idx_lo = ((data1 & 0x0f) << 4) | (data2 & 0x0f);
                    row[sx >> 2] = tile_decode_packed[idx_lo];
                    sx += 4;
                    win_x += 4;
                }

                // render full aligned window tiles
                while (sx + 8 <= 160) {
                    int tile_col = (win_x >> 3) & 31;
                    int tile_idx = vram[win_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    render_tile_row_packed(row + (sx >> 2), data1, data2);
                    sx += 8;
                    win_x += 8;
                }

                // handle partial last window tile
                if (sx < 160) {
                    int tile_col = (win_x >> 3) & 31;
                    int tile_idx = vram[win_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    int idx_hi = (data1 & 0xf0) | (data2 >> 4);
                    row[sx >> 2] = tile_decode_packed[idx_hi];
                }
            } else {
                // slow path: not aligned, use per-pixel rendering
                while (sx < 160) {
                    int tile_col = (win_x >> 3) & 31;
                    int pixel_in_tile = win_x & 7;
                    int pixels_in_tile = 8 - pixel_in_tile;
                    if (sx + pixels_in_tile > 160) {
                        pixels_in_tile = 160 - sx;
                    }

                    int tile_idx = vram[win_map_off + tile_row * 32 + tile_col];
                    int tile_off = unsigned_mode
                        ? tile_base_off + 16 * tile_idx
                        : tile_base_off + 16 * (signed char) tile_idx;

                    u8 data1 = vram[tile_off + row_in_tile * 2];
                    u8 data2 = vram[tile_off + row_in_tile * 2 + 1];

                    render_partial_start(row, data1, data2, pixel_in_tile, pixels_in_tile, sx);
                    sx += pixels_in_tile;
                    win_x += pixels_in_tile;
                }
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

            u8 *row = pixels + row_y * 40;
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
                        int px = lcd_x + x;
                        int byte_idx = px >> 2;
                        int bit_idx = px & 3;
                        row[byte_idx] = packed_set_pixel(row[byte_idx], bit_idx, pal[col_index]);
                    }
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
                        int px = lcd_x + x;
                        int byte_idx = px >> 2;
                        int bit_idx = px & 3;
                        row[byte_idx] = packed_set_pixel(row[byte_idx], bit_idx, pal[col_index]);
                    }
                }
            }
        }
    }
}