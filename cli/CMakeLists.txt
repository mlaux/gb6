project(GameBoyEmulator)
cmake_minimum_required(VERSION 3.9)

include_directories(../src)
include_directories(../include)

execute_process(COMMAND sdl2-config --cflags 
                OUTPUT_VARIABLE SDL_CFLAGS 
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND sdl2-config --libs 
                OUTPUT_VARIABLE SDL_LIBS 
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_CXX_FLAGS "-std=c++14 ${SDL_CFLAGS}")

add_executable(gb6
    ../src/bootstrap.c
    ../src/cpu.c
    ../src/dmg.c
    ../src/instructions.c
    ../src/lcd.c
    ../src/rom.c
    emulator.c
    lcd_imgui.c
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_sdl.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui_example.cpp
)

target_link_libraries(gb6
    "-framework OpenGL"
    "-framework CoreFoundation"
    "${SDL_LIBS}"
)

