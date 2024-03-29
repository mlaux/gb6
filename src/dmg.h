#ifndef _DMG_H
#define _DMG_H

#include "types.h"

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

#define REG_TIMER_DIV 0xFF04
#define REG_TIMER_COUNT 0xFF05
#define REG_TIMER_MOD 0xFF06
#define REG_TIMER_CONTROL 0xFF07

#define TIMER_CONTROL_ENABLED (1 << 2)

struct cpu;
struct rom;
struct lcd;

struct dmg {
    struct cpu *cpu;
    struct rom *rom;
    struct lcd *lcd;
    u8 main_ram[0x2000];
    u8 video_ram[0x2000];
    u8 zero_page[0x80];
    u32 last_lcd_update;
    u32 last_timer_update;
    int joypad_selected;
    int action_selected;
    u8 interrupt_enabled;
    u8 interrupt_requested;

    u8 joypad;
    u8 action_buttons;
    u16 timer_div;
    u8 timer_count;
    u8 timer_mod;
    u8 timer_control;
};

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom, struct lcd *lcd);
void dmg_set_button(struct dmg *dmg, int field, int button, int pressed);

// why did i make these void *
u8 dmg_read(void *dmg, u16 address);
void dmg_write(void *dmg, u16 address, u8 data);

void dmg_step(void *dmg);

#endif
