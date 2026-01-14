#include "../src/dmg.h"
#include "emulator.h"
#include "input.h"
#include "dialogs.h"

// indices match keyMappings order
// (up, down, left, right, a, b, select, start)
static struct {
  int button;
  int field;
} buttonMap[8] = {
  { BUTTON_UP, FIELD_JOY },
  { BUTTON_DOWN, FIELD_JOY },
  { BUTTON_LEFT, FIELD_JOY },
  { BUTTON_RIGHT, FIELD_JOY },
  { BUTTON_A, FIELD_ACTION },
  { BUTTON_B, FIELD_ACTION },
  { BUTTON_SELECT, FIELD_ACTION },
  { BUTTON_START, FIELD_ACTION }
};

void HandleKeyEvent(int keyCode, int down)
{
  int k;

  for (k = 0; k < 8; k++) {
    if (keyMappings[k] == keyCode) {
      dmg_set_button(&dmg, buttonMap[k].field, buttonMap[k].button, down);
      break;
    }
  }
}
