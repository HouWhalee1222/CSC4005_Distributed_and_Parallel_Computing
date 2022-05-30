#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <cstring>
#include <nbody/body.hpp>
#include <chrono>

#define ITERATION 100

template <typename ...Args>
void UNUSED(Args&&... args [[maybe_unused]]) {}

int main(int argc, char **argv) {
    UNUSED(argc, argv);

    if (argc == 1) {
        thread_num = 1;  // sequential by default
    } else if (argc == 2) {
        thread_num = atoi(*(argv + 1));
    } else {
        std::cerr << "usage: " << argv[0] << " <thread number> " << std::endl;
        return 0;
    }

    omp_set_num_threads(thread_num);

    // Meta data
    static float gravity = 100;
    static float space = 800;
    static float radius = 5;
    static int bodies = 200;
    static float elapse = 0.01;
    static ImVec4 color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
    static float max_mass = 50;
    static float current_space = space;
    static float current_max_mass = max_mass;
    static int current_bodies = bodies;

    BodyPool pool(static_cast<size_t>(bodies), space, max_mass);
    
    size_t duration = 0;
    int count = 0;  // The number of iteration
    
    graphic::GraphicContext context{"Assignment 3"};
    context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {
        auto io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Assignment 3", nullptr,
                     ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoCollapse
                     | ImGuiWindowFlags_NoTitleBar
                     | ImGuiWindowFlags_NoResize);
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        ImGui::DragFloat("Space", &current_space, 10, 200, 1600, "%f");
        ImGui::DragFloat("Gravity", &gravity, 0.5, 0, 1000, "%f");
        ImGui::DragFloat("Radius", &radius, 0.5, 2, 20, "%f");
        ImGui::DragInt("Bodies", &current_bodies, 1, 2, 100, "%d");
        ImGui::DragFloat("Elapse", &elapse, 0.001, 0.001, 10, "%f");
        ImGui::DragFloat("Max Mass", &current_max_mass, 0.5, 5, 100, "%f");
        ImGui::ColorEdit4("Color", &color.x);
        
        if (current_space != space || current_bodies != bodies || current_max_mass != max_mass) {
            space = current_space;
            bodies = current_bodies;
            max_mass = current_max_mass;
            pool = BodyPool{static_cast<size_t>(bodies), space, max_mass};
        }

        {
            using namespace std::chrono;

            const ImVec2 p = ImGui::GetCursorScreenPos();

            /* Start calculation */
            auto begin = high_resolution_clock::now();

            pool.update_for_tick(elapse, gravity, space, radius);
            
            auto end = high_resolution_clock::now();
            /* Finish calculation */            

            count++;
            duration += duration_cast<nanoseconds>(end - begin).count();
            if (count == ITERATION) {
                std::cout << ITERATION << " elapse with " << duration << " nanoseconds\n";
                double speed = double(ITERATION) / double(duration) * 1e9;
                std::cout << "speed: " << speed << " iterations per second" << std::endl;
                count = 0;
                duration = 0;
            }

            for (size_t i = 0; i < pool.size(); ++i) {
                auto body = pool.get_body(i);
                auto x = p.x + static_cast<float>(body.get_x());
                auto y = p.y + static_cast<float>(body.get_y());
                draw_list->AddCircleFilled(ImVec2(x, y), radius, ImColor{color});
            }
        }
        ImGui::End();
    });

    return 0;
}
