#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void lcd_new(struct lcd *lcd)
{
    lcd->buf = malloc(256 * 256);
    // todo < 8 bpp
    lcd->pixels = malloc(LCD_WIDTH * LCD_HEIGHT);
}

u8 lcd_is_valid_addr(u16 addr)
{
    return (addr >= 0xfe00 && addr < 0xfea0) || (addr >= REG_LCD_BASE && addr <= REG_LCD_LAST);
}

u8 lcd_read(struct lcd *lcd, u16 addr)
{
    if (addr >= 0xfe00 && addr < 0xfea0) {
        return lcd->oam[addr - 0xfe00];
    }
    return lcd->regs[addr - REG_LCD_BASE];
}

void lcd_write(struct lcd *lcd, u16 addr, u8 value)
{
    if (addr >= 0xfe00 && addr < 0xfea0) {
        lcd->oam[addr - 0xfe00] = value;
    } else {
        lcd->regs[addr - REG_LCD_BASE] = value;
        if (addr == 0xFF46) {
            // OAM DMA
        }
    }
}

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        printf("warning: trying to write to (%d, %d) outside of lcd bounds\n", x, y);
        return;
    }
    lcd->pixels[y * LCD_WIDTH + x] = value;
}

void lcd_copy(struct lcd *lcd)
{
    // use all the registers to compute the pixel data
    
}

int lcd_step(struct lcd *lcd)
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

    return next_scanline;
    // printf("update lcd %d\n", next_scanline);
}