#ifndef _CPU_H
#define _CPU_H

#include "types.h"

struct dmg;

struct cpu
{
    u16 pc;
    u8 interrupt_enable;
    struct dmg *dmg;
};

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
