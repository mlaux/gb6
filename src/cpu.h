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

#define FLAG_Z(cpu) ((cpu)->f >> 7 & 1)
#define FLAG_N(cpu) ((cpu)->f >> 6 & 1)
#define FLAG_H(cpu) ((cpu)->f >> 5 & 1)
#define FLAG_C(cpu) ((cpu)->f >> 4 & 1)

#endif
