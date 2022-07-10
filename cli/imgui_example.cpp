// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_memory_editor.h"
#include <stdio.h>
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif
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

static MemoryEditor editor;

GLuint make_output_texture() {
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    return image_texture;
}

unsigned char output_image[160 * 144 * 4];

void convert_output(struct lcd *lcd) {
    int x, y;
    int out_index = 0;
    for (y = 0; y < 144; y++) {
        for (x = 0; x < 160; x++) {
            int val = lcd->pixels[y * 160 + x];
            int fill = val ? 255 : 0;
            output_image[out_index++] = val;
            output_image[out_index++] = val;
            output_image[out_index++] = val;
            output_image[out_index++] = 255;
        }
    }
}

char full_address_space[0x10000];
void fill_memory_editor(struct dmg *dmg)
{
    int k;
    for (k = 0; k < 0x10000; k++) {
        full_address_space[k] = dmg_read(dmg, k);
    }
}

// Main code
int main(int argc, char *argv[])
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

    // this might be too much abstraction but it'll let me
    // test the cpu, rom, and dmg independently and use the cpu
    // for other non-GB stuff
    dmg_new(&dmg, &cpu, &rom, &lcd);
    cpu_bind_mem_model(&cpu, &dmg, dmg_read, dmg_write);

    cpu.pc = 0x100;

    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
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
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(0); // disable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // setup output
    GLuint texture = make_output_texture();

    // Our state
    bool z_flag = false;
    bool n_flag = false;
    bool h_flag = false;
    bool c_flag = false;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        dmg_step(&dmg);

        z_flag = flag_isset(dmg.cpu, FLAG_ZERO);
        n_flag = flag_isset(dmg.cpu, FLAG_SIGN);
        h_flag = flag_isset(dmg.cpu, FLAG_HALF_CARRY);
        c_flag = flag_isset(dmg.cpu, FLAG_CARRY);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            ImGui::Begin("State");                          // Create a window called "Hello, world!" and append into it.

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

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        {
            ImGui::Begin("Output");

            convert_output(dmg.lcd);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 160, 144, 0, GL_RGBA, GL_UNSIGNED_BYTE, output_image);
            ImGui::Image((void*)(intptr_t) texture, ImVec2(160, 144));

            ImGui::End();
        }

        fill_memory_editor(&dmg);

        editor.DrawWindow("Memory", full_address_space, 0x10000, 0x0000);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    rom_free(&rom);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
