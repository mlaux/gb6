#ifndef _INSTRUCTIONS_H
#define _INSTRUCTIONS_H

struct instruction {
    int opcode;
    const char *format;
};

extern const struct instruction instructions[];

#endif