#ifndef _ROM_H
#define _ROM_H

#include "types.h"

struct rom {
    u32 length;
    int type;
    u8 *data;
};

int rom_load(struct rom *rom, const char *filename);

void rom_free(struct rom *rom);

#endif
