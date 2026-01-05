#ifndef _INSTRUCTIONS_H
#define _INSTRUCTIONS_H

struct instruction {
    int opcode;
    const char *format;
    int cycles;
    int cycles_branch;
};

extern const struct instruction instructions[];

#endif