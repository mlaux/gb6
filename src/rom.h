#ifndef _ROM_H
#define _ROM_H

#include "types.h"
#include "mbc.h"

struct rom {
    u32 length;
    u8 *data;
    struct mbc *mbc;
};

int rom_load(struct rom *rom, const char *filename);

void rom_free(struct rom *rom);

#endif
