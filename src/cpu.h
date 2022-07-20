#ifndef _CPU_H
#define _CPU_H

#include "types.h"

struct cpu
{
    u8 a;
    u8 f;
    u8 b;
    u8 c;
    u8 d;
    u8 e;
    u8 h;
    u8 l;
    u16 sp;
    u16 pc;
    u32 cycle_count;
    u8 interrupt_enable;

    u8 (*mem_read)(void *, u16);
    void (*mem_write)(void *, u16, u8);
    void *mem_model;
};

void cpu_bind_mem_model(
    struct cpu *cpu,
    void *mem_model,
    u8 (*mem_read)(void *, u16),
    void (*mem_write)(void *, u16, u8)
);

void cpu_step(struct cpu *cpu);
int flag_isset(struct cpu *cpu, int flag);

#define FLAG_ZERO       0x80
#define FLAG_SIGN       0x40
#define FLAG_HALF_CARRY 0x20
#define FLAG_CARRY      0x10

#define INT_VBLANK  (1 << 0)
#define INT_LCDSTAT (1 << 1)
#define INT_TIMER   (1 << 2)
#define INT_SERIAL  (1 << 3)
#define INT_JOYPAD  (1 << 4)
#define NUM_INTERRUPTS 5

#endif
