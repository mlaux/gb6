#ifndef _LCD_H
#define _LCD_H

#include "types.h"

#define LCD_WIDTH 160
#define LCD_HEIGHT 144

struct lcd {
    u8 *pixels;
};

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value);

#endif