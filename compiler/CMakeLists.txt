project(CompilerTests)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_FLAGS "-O2 -Wall") # -DDEBUG_COMPILE")

add_application(CompilerTests
    compiler.c
    emitters.c
    branches.c
    interop.c
    flags.c
    tests/test_native.c
    tests/test_unit.c
    tests/test_exec.c
    CONSOLE
)
