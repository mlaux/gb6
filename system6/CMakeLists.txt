project(GameBoyEmulator)
cmake_minimum_required(VERSION 3.9)

include_directories(../src)

add_application(Emulator
    ../src/bootstrap.c
    ../src/cpu.c
    ../src/dmg.c
    ../src/instructions.c
    ../src/lcd.c
    ../src/rom.c
    emulator.c
    resources.r
)
