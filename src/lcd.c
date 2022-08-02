#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "lcd.h"

void lcd_set_bit(struct lcd *lcd, u16 addr, u8 bit)
{
    lcd_write(lcd, addr, lcd_read(lcd, addr) | (1 << bit));
}

void lcd_clear_bit(struct lcd *lcd, u16 addr, u8 bit)
{
    lcd_write(lcd, addr, lcd_read(lcd, addr) & ~(1 << bit));
}

int lcd_isset(struct lcd *lcd, u16 addr, u8 bit)
{
    u8 val = lcd_read(lcd, addr);
    return val & (1 << bit);
}

void lcd_set_mode(struct lcd *lcd, int mode)
{
    u8 val = lcd_read(lcd, REG_STAT);
    lcd_write(lcd, REG_STAT, (val & 0xfc) | mode);
}

void lcd_new(struct lcd *lcd)
{
    lcd->buf = malloc(256 * 256);
    memset(lcd->buf, 0, 256 * 256);
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

void lcd_apply_scroll(struct lcd *lcd)
{
    int scroll_y = lcd_read(lcd, REG_SCY);
    int scroll_x = lcd_read(lcd, REG_SCX);

    int lines;
    for (lines = 0; lines < 144; lines++) {
        int src_y = (scroll_y + lines) & 0xff;
        int cols;
        for (cols = 0; cols < 160; cols++) {
            int src_off = (src_y << 8) + ((scroll_x + cols) & 0xff);
            lcd->pixels[lines * 160 + cols] = lcd->buf[src_off];
        }
    }
}

int lcd_step(struct lcd *lcd)
{
    // step to next scanline 0-153
    u8 next_scanline = (lcd_read(lcd, REG_LY) + 1) % 154;
    lcd_write(lcd, REG_LY, next_scanline);

    return next_scanline;
    // printf("update lcd %d\n", next_scanline);
}