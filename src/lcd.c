#include <stdio.h>

#include "types.h"
#include "lcd.h"

static void set_bit(struct lcd *lcd, u16 addr, u8 bit)
{
    lcd_write(lcd, addr, lcd_read(lcd, addr) | (1 << bit));
}

static void clear_bit(struct lcd *lcd, u16 addr, u8 bit)
{
    lcd_write(lcd, addr, lcd_read(lcd, addr) & ~(1 << bit));
}

u8 lcd_is_valid_addr(u16 addr)
{
    return addr >= REG_LCD_BASE && addr <= REG_LCD_LAST;
}

u8 lcd_read(struct lcd *lcd, u16 addr)
{
    return lcd->regs[addr - REG_LCD_BASE];
}

void lcd_write(struct lcd *lcd, u16 addr, u8 value)
{
    if (addr == REG_LY) {
        // writing to this register always resets it
        value = 0;
    }
    lcd->regs[addr - REG_LCD_BASE] = value;
}

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        printf("warning: trying to write to (%d, %d) outside of lcd bounds\n", x, y);
        return;
    }
    lcd->pixels[y * LCD_WIDTH + x] = value;
}

void lcd_step(struct lcd *lcd)
{
    // update LYC
    if (lcd_read(lcd, REG_LY) == lcd_read(lcd, REG_LYC)) {
        set_bit(lcd, REG_STAT, STAT_FLAG_MATCH);
    } else {
        clear_bit(lcd, REG_STAT, STAT_FLAG_MATCH);
    }

    // step to next scanline 0-153
    u8 next_scanline = (lcd_read(lcd, REG_LY) + 1) % 154;
    lcd_write(lcd, REG_LY, next_scanline);
}