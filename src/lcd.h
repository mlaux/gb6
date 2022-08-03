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

#define STAT_FLAG_MATCH (1 << 2)
#define STAT_INTR_SOURCE_HBLANK (1 << 3)
#define STAT_INTR_SOURCE_VBLANK (1 << 4)
#define STAT_INTR_SOURCE_MODE2 (1 << 5)
#define STAT_INTR_SOURCE_MATCH (1 << 6)


#define LCDC_ENABLE_BG (1 << 0)
#define LCDC_ENABLE_OBJ (1 << 1)
#define LCDC_OBJ_SIZE (1 << 2)
#define LCDC_BG_TILE_MAP (1 << 3)
#define LCDC_BG_TILE_DATA (1 << 4)
#define LCDC_ENABLE_WINDOW (1 << 5)
#define LCDC_WINDOW_TILE_MAP (1 << 6)
#define LCDC_ENABLE (1 << 7)

#define OAM_ATTR_PALETTE (1 << 4)
#define OAM_ATTR_MIRROR_X (1 << 5)
#define OAM_ATTR_MIRROR_Y (1 << 6)
#define OAM_ATTR_BEHIND_BG (1 << 7)

struct lcd {
    u8 oam[0xa0];
    u8 regs[0x0c];
    u8 *buf; // 256x256
    u8 *pixels; // the actual 160x144 visible area
};

void lcd_new(struct lcd *lcd);
u8 lcd_is_valid_addr(u16 addr);
u8 lcd_read(struct lcd *lcd, u16 addr);
void lcd_write(struct lcd *lcd, u16 addr, u8 value);

void lcd_put_pixel(struct lcd *lcd, u8 x, u8 y, u8 value);

void lcd_set_bit(struct lcd *lcd, u16 addr, u8 bit);
void lcd_clear_bit(struct lcd *lcd, u16 addr, u8 bit);
int lcd_isset(struct lcd *lcd, u16 addr, u8 bit);
void lcd_set_mode(struct lcd *lcd, int mode);

int lcd_step(struct lcd *lcd);
void lcd_apply_scroll(struct lcd *lcd);

// output the pixels to the screen
void lcd_draw(struct lcd *lcd);

#endif