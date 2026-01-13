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
struct audio;

struct dmg {
    u8 zero_page[0x80];
    // page table for fast memory access (256 pages of 256 bytes each)
    u8 *read_page[256];
    u8 *write_page[256];

    struct cpu *cpu;
    struct rom *rom;
    struct lcd *lcd;
    struct audio *audio;

    u8 main_ram[0x2000];
    u8 video_ram[0x2000];
    u32 frames_rendered;
    u32 frame_skip;
    int joypad_selected;
    int action_selected;
    u8 interrupt_enable;
    u8 interrupt_request_mask;
    void (*rom_bank_switch_hook)(int new_bank);

    u8 joypad;
    u8 action_buttons;
    u16 timer_div;
    u8 timer_count;
    u8 timer_mod;
    u8 timer_control;

    // cycle accumulator for timing - only sync LCD when >= CYCLES_PER_FRAME
    u32 cycles_since_render;
    u8 ly_hack;
    u8 sent_ly_interrupt;
    u8 sent_vblank_start;

    // for DIV evaluation, TODO do the same thing for LY/STAT
    u32 total_cycles;
    u32 div_reset_cycle;
};

void dmg_new(struct dmg *dmg, struct cpu *cpu, struct rom *rom, struct lcd *lcd);
void dmg_set_button(struct dmg *dmg, int field, int button, int pressed);

u8 dmg_read(void *dmg, u16 address);
void dmg_write(void *dmg, u16 address, u8 data);

u8 dmg_read_slow(struct dmg *dmg, u16 address);
void dmg_write_slow(struct dmg *dmg, u16 address, u8 data);

void dmg_sync_hw(struct dmg *dmg, int cycles);
void dmg_request_interrupt(struct dmg *dmg, int nr);

// page table management
void dmg_init_pages(struct dmg *dmg);
void dmg_update_rom_bank(struct dmg *dmg, int bank);
void dmg_update_ram_bank(struct dmg *dmg, u8 *ram_base);

void dmg_ei_di(void *dmg, u16 enabled);

#endif
