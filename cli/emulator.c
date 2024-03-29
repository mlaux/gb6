#include <stdio.h>

#include "dmg.h"
#include "cpu.h"
#include "rom.h"
#include "lcd.h"

int emulator_main(int argc, char *argv[])
{
    struct cpu cpu;
    struct rom rom;
    struct dmg dmg;
    struct lcd lcd;

    int executed;

    if (argc < 2) {
        printf("no rom specified\n");
        return 1;
    }

    if (!rom_load(&rom, argv[1])) {
        printf("error loading rom\n");
        return 1;
    }

    lcd_new(&lcd);

    dmg_new(&dmg, &cpu, &rom, &lcd);
    cpu.dmg = &dmg;
    // cpu_bind_mem_model(&cpu, &dmg, dmg_read, dmg_write);

    cpu.pc = 0x100;

    // for (executed = 0; executed < 1000; executed++) {
    while (1) {
        dmg_step(&dmg);
    }

    rom_free(&rom);
    return 0;
}
