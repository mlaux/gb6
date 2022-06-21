#ifndef _LCD_H
#define _LCD_H

#include "types.h"

#define LCD_WIDTH 160
#define LCD_HEIGHT 144

#define REG_LCD_BASE 0xff40

#define REG_LCDC 0xff40
#define REG_STAT 0xff41
#define REG_SCY  0xff42
#define REG_SCX  0xff43
#define REG_LY   0xff44
#define REG_LYC  0xff45
#define REG_DMA  0xff46
#define REG_BGP  0xff47
#define REG_OBP0 0xff48
#define REG_OBP1 0xff49
#define REG_WY   0xff4a
#define REG_WX   0xff4b

#define REG_LCD_LAST REG_WX

#define STAT_FLAG_MATCH 2

struct lcd {
    u8 regs[0x0c];
    u8 *pixels;
};

void lcd_new(struct lcd *lcd);
u8 lcd_is_valid_addr(u16 addr);
u8 lcd_read(struct lcd *lcd, u16 addr);
void lcd_write(struct lcd *lcd, u16 addr, u8 value);

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value);

// i feel like i'm going to need to call this every cycle and update regs
void lcd_step(struct lcd *lcd);

#endif