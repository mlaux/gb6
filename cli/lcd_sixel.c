#include <stdio.h>
#include <string.h>
#include "../src/lcd.h"

void lcd_draw(struct lcd *lcd)
{
  int x, y, yy;

  puts("\033[2J\033[H"); // clear screen and move cursor to 0, 0
  puts("\033Pq"); // enter sixel graphics mode
  for (y = 0; y < LCD_HEIGHT; y += 6) {
    for (x = 0; x < LCD_WIDTH; x++) {
      int val = 63;
      for (yy = 0; yy < 6; yy++) {
        val += !lcd->pixels[(y + yy) * LCD_WIDTH + x] << yy;
      }
      putchar(val);
    }
    putchar('-');
  }
  // puts("#0;2;0;0;0#1;2;100;100;0#2;2;0;100;0");
  // puts("#1~~@@vv@@~~@@~~$");
  // puts("#2??}}GG}}??}}??-");
  // puts("#1!14@");
  puts("\033\\"); // leave graphics mode
}

int test_main(int argc, char *argv[])
{
  struct lcd lcd;
  lcd_new(&lcd);
  lcd_draw(&lcd);
  return 0;
}