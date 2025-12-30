/* Game Boy emulator for 68k Macs
   lcd_mac.c - LCD rendering with 2x dithering */

#ifndef UNITY_BUILD
#include <Quickdraw.h>
#include <Windows.h>
#include "../src/lcd.h"
#endif

extern WindowPtr g_wp;
extern char offscreen_buf[];
extern Rect offscreen_rect;
extern BitMap offscreen_bmp;

// lookup tables for 2x dithered rendering
// index = 4 packed GB pixels (2 bits each), output = 8 screen pixels (1bpp)
static unsigned char dither_row0[256];
static unsigned char dither_row1[256];

// FPS tracking
static unsigned long fps_frame_count = 0;
static unsigned long fps_last_tick = 0;
static unsigned long fps_display = 0;
extern char status_bar[64];

void init_dither_lut(void)
{
  // 2x2 dither patterns for each GB color (0-3)
  // stored in high 2 bits: 00=white, 01=right black, 10=left black, 11=both black
  // color 0 (white):      top=00 bot=00
  // color 1 (light gray): top=00 bot=01 (one black pixel, bottom-right)
  // color 2 (dark gray):  top=10 bot=01 (checkerboard)
  // color 3 (black):      top=11 bot=11
  static const unsigned char pat_top[4] = { 0x00, 0x00, 0x80, 0xc0 };
  static const unsigned char pat_bot[4] = { 0x00, 0x40, 0x40, 0xc0 };
  int idx;

  for (idx = 0; idx < 256; idx++) {
    int p0 = (idx >> 6) & 3;
    int p1 = (idx >> 4) & 3;
    int p2 = (idx >> 2) & 3;
    int p3 = idx & 3;

    dither_row0[idx] = pat_top[p0] | (pat_top[p1] >> 2) |
                       (pat_top[p2] >> 4) | (pat_top[p3] >> 6);
    dither_row1[idx] = pat_bot[p0] | (pat_bot[p1] >> 2) |
                       (pat_bot[p2] >> 4) | (pat_bot[p3] >> 6);
  }
}

// called by dmg_step at vblank
void lcd_draw(struct lcd *lcd_ptr)
{
  int gy;
  unsigned char *src = lcd_ptr->pixels;
  unsigned char *dst = (unsigned char *) offscreen_buf;

  for (gy = 0; gy < 144; gy++) {
    unsigned char *row0 = dst;
    unsigned char *row1 = dst + 40;
    int gx;

    for (gx = 0; gx < 160; gx += 4) {
      // pack 4 GB pixels into LUT index
      unsigned char idx = (src[0] << 6) | (src[1] << 4) | (src[2] << 2) | src[3];
      src += 4;

      *row0++ = dither_row0[idx];
      *row1++ = dither_row1[idx];
    }

    dst += 80; // advance 2 screen rows
  }

  SetPort(g_wp);
  CopyBits(&offscreen_bmp, &g_wp->portBits, &offscreen_rect, &offscreen_rect, srcCopy, NULL);

  // FPS display
  {
    unsigned long now = TickCount();
    Str255 fpsStr;

    fps_frame_count++;
    if (now - fps_last_tick >= 60) {
      fps_display = fps_frame_count;
      fps_frame_count = 0;
      fps_last_tick = now;
    }

    {
      Rect statusRect = { 288, 0, 299, 320 };
      EraseRect(&statusRect);
    }
    MoveTo(4, 298);

    if (status_bar[0]) {
      // Convert C string to Pascal string and draw
      Str255 pstr;
      int k;
      for (k = 0; k < 255 && status_bar[k]; k++) {
        pstr[k + 1] = status_bar[k];
      }
      pstr[0] = k;
      DrawString(pstr);
    } else {
      NumToString(fps_display, fpsStr);
      DrawString("\pFPS: ");
      DrawString(fpsStr);
    }
  }
}