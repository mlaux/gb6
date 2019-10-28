project(GameBoyEmulator)
cmake_minimum_required(VERSION 3.9)

include_directories(../src)
include_directories(../include)
link_directories(../lib)

add_executable(gb6
    ../src/bootstrap.c
    ../src/cpu.c
    ../src/dmg.c
    ../src/instructions.c
    ../src/lcd.c
    ../src/rom.c
    emulator.c
    gl3w.c
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.cpp
    imgui_widgets.cpp
    main.cpp
)

target_link_libraries(gb6 
    glfw3
    "-framework CoreFoundation"
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
)

