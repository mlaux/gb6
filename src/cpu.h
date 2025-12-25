#ifndef _CPU_H
#define _CPU_H

#include "types.h"

struct dmg;

struct cpu
{
    union {
        u16 af;
        struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            u8 f; u8 a;
#else
            u8 a; u8 f;
#endif
        };
    };
    union {
        u16 bc;
        struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            u8 c; u8 b;
#else
            u8 b; u8 c;
#endif
        };
    };
    union {
        u16 de;
        struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            u8 e; u8 d;
#else
            u8 d; u8 e;
#endif
        };
    };
    union {
        u16 hl;
        struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            u8 l; u8 h;
#else
            u8 h; u8 l;
#endif
        };
    };
    u16 sp;
    u16 pc;
    u32 cycle_count;
    u8 interrupt_enable;

    u8 halted;

    struct dmg *dmg;
};

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
