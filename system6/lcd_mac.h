#ifndef _LCD_MAC_H
#define _LCD_MAC_H

#include <Quickdraw.h>

typedef struct {
    const char *name;
    RGBColor colors[4];
} GBPalette;

extern int current_palette;

void init_dither_lut(void);
void init_indexed_lut(WindowPtr wp);

struct lcd;
void lcd_draw(struct lcd *lcd_ptr);

#endif