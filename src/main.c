#include <stdio.h>

#include "dmg.h"
#include "cpu.h"
#include "rom.h"

int main(int argc, char *argv[])
{
    struct cpu cpu;
    struct rom rom;
    struct dmg dmg;

    if (argc < 2) {
        printf("no rom specified\n");
        return 1;
    }

    if (!rom_load(&rom, argv[1])) {
        printf("error loading rom\n");
        return 1;
    }   

    // this might be too much abstraction but it'll let me
    // test the cpu, rom, and dmg independently and use the cpu
    // for other non-GB stuff
    dmg_new(&dmg, &cpu, &rom);
    cpu_bind_mem_model(&cpu, &dmg, dmg_read, dmg_write);

    cpu.pc = 0;

    while (1) {
        cpu_step(&cpu);
    }

    rom_free(&rom);
    return 0;
}
