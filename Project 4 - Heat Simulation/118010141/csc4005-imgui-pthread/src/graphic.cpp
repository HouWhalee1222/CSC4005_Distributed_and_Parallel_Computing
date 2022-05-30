//
// Created by schrodinger on 9/9/21.
//
#include <graphic/graphic.hpp>

graphic::GraphicContext::GraphicContext(std::string title, int height, int width, graphic::VSyncFlag vsync_flag,
                                        ImVec4 clear_color) :
        title_(std::move(title)), clear_color_(clear_color) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        throw GraphicException("unable to init SDL");
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                                     SDL_WINDOW_ALLOW_HIGHDPI);
    sdl_window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    gl_context = SDL_GL_CreateContext(sdl_window);
    SDL_GL_MakeCurrent(sdl_window, gl_context);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(sdl_window, gl_context);
    ImGui_ImplOpenGL2_Init();
    SDL_GL_SetSwapInterval(static_cast<int>(vsync_flag));
}

graphic::GraphicContext::~GraphicContext() {
    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}
