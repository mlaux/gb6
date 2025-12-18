#include "../src/dmg.h"
#include "../src/lcd.h"

unsigned int default_palette[] = { 0x9ff4e5, 0x00b9be, 0x005f8c, 0x002b59 };
// ugly evenly spaced: { 0xffffff, 0xaaaaaa, 0x555555, 0x000000 };
// bgb default: { 0xe0f8d0, 0x88c070, 0x346856, 0x081820 };

// https://lospec.com/palette-list/blk-aqu4
// { 0x9ff4e5, 0x00b9be, 0x005f8c, 0x002b59 };

// https://lospec.com/palette-list/velvet-cherry-gb
// { 0x9775a6, 0x683a68, 0x412752, 0x2d162c };

extern unsigned char visible_pixels[160 * 144 * 4];
extern unsigned char vram_tiles[256 * 96 * 4];

void lcd_draw(struct lcd *lcd)
{   
    int x, y;
    int out_index = 0;
    for (y = 0; y < 144; y++) {
        for (x = 0; x < 160; x++) {
            int val = lcd->pixels[y * 160 + x];
            int fill = default_palette[val];
            visible_pixels[out_index++] = (fill >> 16) & 0xff;
            visible_pixels[out_index++] = (fill >> 8) & 0xff;
            visible_pixels[out_index++] = fill & 0xff;
            visible_pixels[out_index++] = 255;
        }
    }
}

void convert_vram(struct dmg *dmg) {
    int tile_y, tile_x;
    int off, in;
    for (tile_y = 0; tile_y < 12; tile_y++) {
        for (tile_x = 0; tile_x < 32; tile_x++) {
            off = 256 * 8 * tile_y + 8 * tile_x;
            in = 16 * (tile_y * 32 + tile_x);
            int b, i;
            for (b = 0; b < 16; b += 2) {
                int data1 = dmg->video_ram[in + b];
                int data2 = dmg->video_ram[in + b + 1];
                for (i = 7; i >= 0; i--) {
                    int fill = (data1 & (1 << i)) ? 1 : 0;
                    fill |= ((data2 & (1 << i)) ? 1 : 0) << 1;
                    fill = default_palette[fill];
                    vram_tiles[4 * off + 0] = (fill >> 16) & 0xff;
                    vram_tiles[4 * off + 1] = (fill >> 8) & 0xff;
                    vram_tiles[4 * off + 2] = fill & 0xff;
                    vram_tiles[4 * off + 3] = 255;
                    off++;
                }
                off += 248;
            }
        }
    }
}
