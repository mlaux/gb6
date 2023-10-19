project(GameBoyEmulator)
cmake_minimum_required(VERSION 3.9)

include_directories(../src)

set(CMAKE_C_FLAGS "-O3")

add_application(Emulator
    ../src/bootstrap.c
    ../src/cpu.c
    ../src/dmg.c
    ../src/instructions.c
    ../src/lcd.c
    ../src/rom.c
    ../src/mbc.c
    emulator.c
    lcd_mac.c
    resources.r
)

add_application(CompilerTest
    compiler68k.c
    CONSOLE
)