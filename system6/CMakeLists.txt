project(GameBoyEmulator)
cmake_minimum_required(VERSION 3.9)

include_directories(../src ../compiler)

set(CMAKE_C_FLAGS "-O3")

add_application(Emulator
    ../src/bootstrap.c
    ../src/cpu.c
    ../src/dmg.c
    ../src/instructions.c
    ../src/lcd.c
    ../src/rom.c
    ../src/mbc.c
    ../compiler/compiler.c
    ../compiler/emitters.c
    ../compiler/branches.c
    ../compiler/interop.c
    ../compiler/flags.c
    ../compiler/cb_prefix.c
    emulator.c
    resources.r
)
