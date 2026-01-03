#include "../src/dmg.h"
#include "input.h"

extern struct dmg dmg;

// key input mapping for game controls
// using ASCII character codes (case-insensitive handled in code)
struct key_input {
    char key;
    int button;
    int field;
};

static struct key_input key_inputs[] = {
    { 'd', BUTTON_RIGHT, FIELD_JOY },
    { 'a', BUTTON_LEFT, FIELD_JOY },
    { 'w', BUTTON_UP, FIELD_JOY },
    { 's', BUTTON_DOWN, FIELD_JOY },
    { 'l', BUTTON_A, FIELD_ACTION },
    { 'k', BUTTON_B, FIELD_ACTION },
    { 'n', BUTTON_SELECT, FIELD_ACTION },
    { 'm', BUTTON_START, FIELD_ACTION },
    { 0, 0, 0 }
};

void HandleKeyEvent(int ch, int down)
{
  if (ch >= 'A' && ch <= 'Z') ch += 32; // tolower
  struct key_input *key = key_inputs;
  while (key->key) {
    if (key->key == ch) {
      dmg_set_button(&dmg, key->field, key->button, down);
      break;
    }
    key++;
  }
}
