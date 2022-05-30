//
// Created by schrodinger on 9/9/21.
//
#pragma once

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl2.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <exception>
#include <string>
#include <memory>
#include <iostream>
namespace graphic {

    enum class VSyncFlag : int {
        Adaptive = -1,
        Immediate = 0,
        Synchronized = 1
    };

    class GraphicException : std::exception {
        std::string msg_;
    public:
        explicit GraphicException(std::string msg) : msg_(std::move(msg)) {}

        [[nodiscard]] const char *what() const noexcept override {
            return msg_.c_str();
        }
    };

    class GraphicContext {
        std::string title_;
        ImVec4 clear_color_;
        SDL_Window *sdl_window;
        SDL_GLContext gl_context;
        bool finished = false;
    public:
        explicit GraphicContext(std::string title = "untitled", int height = 1000, int width = 1000,
                                VSyncFlag vsync_flag = VSyncFlag::Immediate,
                                ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f));

        template<class Event>
        void run(Event user_event) {
            auto io = ImGui::GetIO();
#ifdef FONT_PATH
            float ddpi, hdpi, vdpi;
            if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) != 0) {
               std:: cerr << "[WARN] cannot get screen dpi, auto-scaling disabled" << std::endl;
            } else {
               auto font_size = ddpi / 72.f * 10.f;
               auto font = io.Fonts->AddFontFromFileTTF(FONT_PATH, font_size);
               IM_ASSERT(font != NULL);
               IM_UNUSED(font);
            }
#endif
            while (!finished) {
                SDL_SetWindowTitle(sdl_window, title_.c_str());
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    ImGui_ImplSDL2_ProcessEvent(&event);
                    if (event.type == SDL_QUIT)
                        quit();
                    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                        event.window.windowID == SDL_GetWindowID(sdl_window))
                        quit();
                }

                // Start the Dear ImGui frame
                ImGui_ImplOpenGL2_NewFrame();
                ImGui_ImplSDL2_NewFrame();
                ImGui::NewFrame();

                user_event(this, sdl_window);

                ImGui::Render();
                glViewport(0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y);
                glClearColor(clear_color_.x * clear_color_.w, clear_color_.y * clear_color_.w,
                             clear_color_.z * clear_color_.w, clear_color_.w);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
                SDL_GL_SwapWindow(sdl_window);
            }
        }

        std::string &title() {
            return title_;
        }

        ImVec4 &clear_color() {
            return clear_color_;
        }

        void quit() {
            finished = true;
        }

        ~GraphicContext();
    };
}
