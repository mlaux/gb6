project(GameBoyEmulator)
cmake_minimum_required(VERSION 3.9)
add_application(Emulator
    src/bootstrap.c
    src/cpu.c
    src/dmg.c
    src/emulator.c
    src/instructions.c
    src/lcd.c
    src/rom.c
    resources.r
)
