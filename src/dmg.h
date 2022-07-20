#ifndef _DMG_H
#define _DMG_H

#include "cpu.h"
#include "rom.h"
#include "lcd.h"

#define FIELD_JOY 1
#define FIELD_ACTION 2

#define BUTTON_RIGHT (1 << 0)
#define BUTTON_LEFT (1 << 1)
#define BUTTON_UP (1 << 2)
#define BUTTON_DOWN (1 << 3)
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)

struct dmg {
    struct cpu *cpu;
    struct rom *rom;
    struct lcd *lcd;
    u8 main_ram[0x2000];
    u8 video_ram[0x2000];
    u8 zero_page[0x80];
    u32 last_lcd_update;
    int action_selected; // non-0 if A/B/start/select selected, 0 for directions
    u8 interrupt_enabled;
    u8 interrupt_requested;

    u8 joypad;
    u8 action_buttons;
};

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom, struct lcd *lcd);
void dmg_set_button(struct dmg *dmg, int field, int button, int pressed);

// why did i make these void *
u8 dmg_read(void *dmg, u16 address);
void dmg_write(void *dmg, u16 address, u8 data);

void dmg_step(void *dmg);

#endif
