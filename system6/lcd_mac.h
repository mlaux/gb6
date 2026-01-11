#ifndef _LCD_MAC_H
#define _LCD_MAC_H

void init_dither_lut(void);
void init_indexed_lut(WindowPtr wp);

void lcd_draw(struct lcd *lcd_ptr);

#endif