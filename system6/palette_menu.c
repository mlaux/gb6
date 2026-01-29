#include <Quickdraw.h>
#include <Menus.h>
#include <Resources.h>

#include <string.h>

#include "emulator.h"
#include "palette_menu.h"
#include "gb_palettes.h"

// MDEF message constants
#define mDrawMsg 0
#define mChooseMsg 1
#define mSizeMsg 2

#define PALETTE_MENU_WIDTH 170

// palette menu item dimensions
#define PAL_ITEM_HEIGHT 16
#define PAL_SWATCH_SIZE 12
#define PAL_SWATCH_GAP 2
#define PAL_SWATCHES_WIDTH (4 * (PAL_SWATCH_SIZE + PAL_SWATCH_GAP))

static void DrawPaletteItem(Rect *menuRect, int index, int highlighted)
{
  Rect itemRect, swatchRect;
  int itemTop;
  FontInfo fi;
  Str255 pstr;
  const char *name;
  int len, c;

  GetFontInfo(&fi);
  itemTop = menuRect->top + index * PAL_ITEM_HEIGHT;

  itemRect.top = itemTop;
  itemRect.bottom = itemTop + PAL_ITEM_HEIGHT;
  itemRect.left = menuRect->left;
  itemRect.right = menuRect->right;

  // fill background
  if (highlighted) {
    ForeColor(blackColor);
    PaintRect(&itemRect);
  } else {
    EraseRect(&itemRect);
  }

  // draw the four color swatches
  for (c = 0; c < 4; c++) {
    swatchRect.top = itemTop + (PAL_ITEM_HEIGHT - PAL_SWATCH_SIZE) / 2;
    swatchRect.bottom = swatchRect.top + PAL_SWATCH_SIZE;
    swatchRect.left = menuRect->right - PAL_SWATCHES_WIDTH
        + c * (PAL_SWATCH_SIZE + PAL_SWATCH_GAP);
    swatchRect.right = swatchRect.left + PAL_SWATCH_SIZE;

    RGBForeColor(&gb_palettes[index].colors[c]);
    PaintRect(&swatchRect);
    ForeColor(blackColor);
    FrameRect(&swatchRect);
  }

  // draw palette name
  name = gb_palettes[index].name;
  len = strlen(name);
  pstr[0] = len;
  for (c = 0; c < len; c++) {
    pstr[c + 1] = name[c];
  }

  if (highlighted) {
    ForeColor(whiteColor);
  } else {
    ForeColor(blackColor);
  }

  MoveTo(menuRect->left + 15, itemTop + fi.ascent);
  DrawString(pstr);

  // draw checkmark if this is the current palette
  if (index == current_palette) {
    MoveTo(menuRect->left + 3, itemTop + fi.ascent);
    DrawChar(0x12);
  }

  ForeColor(blackColor);
}

static pascal void PaletteMDEF(
    short message,
    MenuHandle theMenu,
    Rect *menuRect,
    Point hitPt,
    short *whichItem
)
{
  int k;

  switch (message) {
    case mSizeMsg:
      (*theMenu)->menuWidth = PALETTE_MENU_WIDTH;
      (*theMenu)->menuHeight = gb_palette_count * PAL_ITEM_HEIGHT;
      break;

    case mDrawMsg:
      for (k = 0; k < gb_palette_count; k++)
        DrawPaletteItem(menuRect, k, 0);
      break;

    case mChooseMsg: {
      short oldItem = *whichItem;
      short newItem = 0;

      if (hitPt.v >= menuRect->top && hitPt.v < menuRect->bottom &&
          hitPt.h >= menuRect->left && hitPt.h < menuRect->right) {
        newItem = (hitPt.v - menuRect->top) / PAL_ITEM_HEIGHT + 1;
        if (newItem > gb_palette_count)
          newItem = 0;
      }

      if (newItem != oldItem) {
        if (oldItem > 0 && oldItem <= gb_palette_count) {
          DrawPaletteItem(menuRect, oldItem - 1, 0);
        }
        if (newItem > 0 && newItem <= gb_palette_count) {
          DrawPaletteItem(menuRect, newItem - 1, 1);
        }
        *whichItem = newItem;
      }
      break;
    }
  }
}

void InstallPalettesMenu(void)
{
  Handle h;
  MenuHandle menu;
  int k;
  Str255 pstr;

  // "the 10-byte stub trick" load stub, patch in custom proc address
  // i got this from Retro68's WDEF example but i'm sure it's older than that
  h = GetResource('MDEF', RES_MDEF_ID);
  if (h == nil)
    return;
  HLock(h);
  *(void **)(*h + 6) = (void *) PaletteMDEF;

  menu = NewMenu(MENU_PALETTES, "\pPalettes");

  // add placeholder items, MDEF will draw the content
  // idk if i actually need to do this
  for (k = 0; k < gb_palette_count; k++) {
    AppendMenu(menu, "\p");
  }

  // replace the default with custom proc
  (*menu)->menuProc = h;
  CalcMenuSize(menu);

  InsertMenu(menu, 0);
}
