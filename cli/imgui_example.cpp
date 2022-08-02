#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_memory_editor.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_opengl.h>
#include <string>

extern "C" {
#include "dmg.h"
#include "cpu.h"
#include "rom.h"
#include "lcd.h"
}

static const char *A_FORMAT = "A: 0x%02x";
static const char *B_FORMAT = "B: 0x%02x";
static const char *C_FORMAT = "C: 0x%02x";
static const char *D_FORMAT = "D: 0x%02x";
static const char *E_FORMAT = "E: 0x%02x";
static const char *H_FORMAT = "H: 0x%02x";
static const char *L_FORMAT = "L: 0x%02x";
static const char *SP_FORMAT = "SP: 0x%02x";
static const char *PC_FORMAT = "PC: 0x%02x";

unsigned char output_image[256 * 256 * 4];
unsigned char vram_tiles[256 * 96 * 4];

struct key_input {
    int scancode;
    int button;
    int field;
};

struct key_input key_inputs[] = {
    { SDL_SCANCODE_D, BUTTON_RIGHT, FIELD_JOY },
    { SDL_SCANCODE_A, BUTTON_LEFT, FIELD_JOY },
    { SDL_SCANCODE_W, BUTTON_UP, FIELD_JOY },
    { SDL_SCANCODE_S, BUTTON_DOWN, FIELD_JOY },
    { SDL_SCANCODE_L, BUTTON_A, FIELD_ACTION },
    { SDL_SCANCODE_K, BUTTON_B, FIELD_ACTION },
    { SDL_SCANCODE_N, BUTTON_SELECT, FIELD_ACTION },
    { SDL_SCANCODE_M, BUTTON_START, FIELD_ACTION },
    { 0 }
};

static MemoryEditor editor;

GLuint make_output_texture() {
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return image_texture;
}

unsigned char default_palette[] = { 0xff, 0xaa, 0x55, 0x00 };

void convert_output(struct lcd *lcd) {
    int x, y;
    int out_index = 0;
    for (y = 0; y < 256; y++) {
        for (x = 0; x < 256; x++) {
            int val = lcd->buf[y * 256 + x];
            int fill = default_palette[val];
            //int fill = val ? 255 : 0;
            output_image[out_index++] = fill;
            output_image[out_index++] = fill;
            output_image[out_index++] = fill;
            output_image[out_index++] = 255;
        }
    }
}

void convert_vram(struct dmg *dmg) {
    int tile_y, tile_x;
    int off, in;
    for (tile_y = 0; tile_y < 12; tile_y++) {
        for (tile_x = 0; tile_x < 32; tile_x++) {
            off = 256 * 8 * tile_y + 8 * tile_x;
            in = 16 * (tile_y * 32 + tile_x);
            int b, i;
            for (b = 0; b < 16; b += 2) {
                int data1 = dmg->video_ram[in + b];
                int data2 = dmg->video_ram[in + b + 1];
                for (i = 7; i >= 0; i--) {
                    // monochrome for now
                    int fill = (data1 & (1 << i)) ? 255 : 0;
                    vram_tiles[4 * off + 0] = fill;
                    vram_tiles[4 * off + 1] = fill;
                    vram_tiles[4 * off + 2] = fill;
                    vram_tiles[4 * off + 3] = 255;
                    //dmg->lcd->buf[off] |= (data2 & (1 << i)) ? 1 : 0;
                    off++;
                }
                off += 248;
            }
        }
    }
}

static ImU8 read_mem(const ImU8* data, size_t off)
{
    return dmg_read((void *) data, (u16) off);
}

static void write_mem(ImU8 *data, size_t off, ImU8 d)
{
    dmg_write(data, (u16) off, d);
}

// Main code
int main(int argc, char *argv[])
{
    struct cpu cpu = { 0 };
    struct rom rom = { 0 };
    struct dmg dmg = { 0 };
    struct lcd lcd = { 0 };

    int executed;
    int paused = 0;
    int pause_next = 0;

    if (argc < 2) {
        printf("no rom specified\n");
        return 1;
    }

    if (!rom_load(&rom, argv[1])) {
        printf("error loading rom\n");
        return 1;
    }

    lcd_new(&lcd);

    // this might be too much abstraction but it'll let me
    // test the cpu, rom, and dmg independently and use the cpu
    // for other non-GB stuff
    dmg_new(&dmg, &cpu, &rom, &lcd);
    cpu_bind_mem_model(&cpu, &dmg, dmg_read, dmg_write);

    cpu.pc = 0x100;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("gb6 debug", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(0); // disable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // setup output
    GLuint texture = make_output_texture();
    GLuint vram_texture = make_output_texture();

    editor.ReadFn = read_mem;
    editor.WriteFn = write_mem;

    // for flag checkboxes
    bool z_flag = false;
    bool n_flag = false;
    bool h_flag = false;
    bool c_flag = false;

    // Main loop
    bool done = false;
    unsigned int lastDrawTime = 0, currentTime;

    while (!done) {
        if (!paused) {
            dmg_step(&dmg);
        }
        if (pause_next) {
            paused = 1;
        }

        currentTime = SDL_GetTicks();
        if (currentTime >= lastDrawTime + 16) {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
            // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) {
                    done = true;
                }

                if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE 
                        && event.window.windowID == SDL_GetWindowID(window)) {
                    done = true;
                }

                if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    struct key_input *key = key_inputs;
                    while (key->scancode) {
                        if (key->scancode == event.key.keysym.scancode) {
                            dmg_set_button(&dmg, key->field, key->button, event.type == SDL_KEYDOWN);
                            break;
                        }
                        key++;
                    }
                }
            }

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            z_flag = flag_isset(dmg.cpu, FLAG_ZERO);
            n_flag = flag_isset(dmg.cpu, FLAG_SIGN);
            h_flag = flag_isset(dmg.cpu, FLAG_HALF_CARRY);
            c_flag = flag_isset(dmg.cpu, FLAG_CARRY);

            {
                ImGui::Begin("State");

                ImGui::Text(A_FORMAT, dmg.cpu->a);
                ImGui::Text(B_FORMAT, dmg.cpu->b);
                ImGui::SameLine();
                ImGui::Text(C_FORMAT, dmg.cpu->c);
                ImGui::Text(D_FORMAT, dmg.cpu->d);
                ImGui::SameLine();
                ImGui::Text(E_FORMAT, dmg.cpu->e);
                ImGui::Text(H_FORMAT, dmg.cpu->h);
                ImGui::SameLine();
                ImGui::Text(L_FORMAT, dmg.cpu->l);
                ImGui::Text(SP_FORMAT, dmg.cpu->sp);
                ImGui::SameLine();
                ImGui::Text(PC_FORMAT, dmg.cpu->pc);

                ImGui::Checkbox("Z", &z_flag);
                ImGui::SameLine();
                ImGui::Checkbox("N", &n_flag);
                ImGui::SameLine();
                ImGui::Checkbox("H", &h_flag);
                ImGui::SameLine();
                ImGui::Checkbox("C", &c_flag);

                if (ImGui::Button("Run")) {
                    paused = 0;
                    pause_next = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    paused = 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Step")) {
                    paused = 0;
                    pause_next = 1;
                }

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();
            }

            {
                ImGui::Begin("Output");

                convert_output(dmg.lcd);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, output_image);
                ImGui::Image((void*)(intptr_t) texture, ImVec2(512, 512));

                ImGui::End();
            }

            {
                ImGui::Begin("VRAM");

                convert_vram(&dmg);
                glBindTexture(GL_TEXTURE_2D, vram_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 96, 0, GL_RGBA, GL_UNSIGNED_BYTE, vram_tiles);
                ImGui::Image((void*)(intptr_t) vram_texture, ImVec2(256, 96));

                ImGui::End();
            }

            editor.DrawWindow("Memory", &dmg, 0x10000, 0x0000);

            // Rendering
            ImGui::Render();
            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);

            lastDrawTime = currentTime;
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    rom_free(&rom);

    return 0;
}
