#include "../src/lcd.h"

unsigned int default_palette[] = { 0x9ff4e5, 0x00b9be, 0x005f8c, 0x002b59 };
// ugly evenly spaced: { 0xffffff, 0xaaaaaa, 0x555555, 0x000000 };
// bgb default: {0xe0f8d0, 0x88c070, 0x346856, 0x081820};

// https://lospec.com/palette-list/blk-aqu4
// { 0x9ff4e5, 0x00b9be, 0x005f8c, 0x002b59 };

// https://lospec.com/palette-list/velvet-cherry-gb
// { 0x9775a6, 0x683a68, 0x412752, 0x2d162c };

extern unsigned char visible_pixels[160 * 144 * 4];

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