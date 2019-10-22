#include <stdio.h>

#include "types.h"
#include "lcd.h"

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        printf("warning: trying to write to (%d, %d) outside of lcd bounds\n", x, y);
        return;
    }
    lcd->pixels[y * LCD_WIDTH + x] = value;
}