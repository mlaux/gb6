#include <stdlib.h>
#include <stdio.h>
#include "rom.h"
#include "types.h"

int rom_load(struct rom *rom, const char *filename)
{
    FILE *fp;
    int len;

    fp = fopen(filename, "r");
    if (!fp) {
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    rewind(fp);

    rom->data = malloc(len);
    rom->length = len;
    if (fread(rom->data, 1, len, fp) < len) {
        return 0;
    }
    rom->mbc = mbc_new(rom->data[0x147]);
    if (!rom->mbc) {
        return 0;
    }

    return 1;
}

void rom_free(struct rom *rom)
{
    free(rom->data);
}
