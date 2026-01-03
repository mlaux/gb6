cmake_minimum_required(VERSION 3.9)
project(GameBoyEmulator)

include_directories(../src ../compiler)

set(CMAKE_C_FLAGS "-O3")

add_application(Emulator
    TYPE APPL
    CREATOR MGBE
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
    ../compiler/reg_loads.c
    ../compiler/alu.c
    dialogs.c
    debug.c
    dispatcher_asm.c
    jit.c
    input.c
    lcd_mac.c
    lru.c
    emulator.c
    resources.r
)
