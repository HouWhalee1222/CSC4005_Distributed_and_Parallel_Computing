#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <cstring>
#include <chrono>
#include <hdist/hdist.hpp>

#include <pthread.h>

#define ITERATION 10

// Global variables
static hdist::State current_state;
auto grid = hdist::Grid{
    static_cast<size_t>(current_state.room_size),
    current_state.border_temp,
    current_state.source_temp,
    static_cast<size_t>(current_state.source_x),
    static_cast<size_t>(current_state.source_y)
};
std::atomic_bool finished{false};

void *thread_calculate(void *) {
    bool thread_finished = hdist::calculate(current_state, grid);
    if (!thread_finished) finished = false;
    return nullptr;
}

ImColor temp_to_color(double temp) {
    auto value = static_cast<uint8_t>(temp / 100.0 * 255.0);
    return {value, 0, 255 - value};
}

template<typename ...Args>
void UNUSED(Args &&... args [[maybe_unused]]) {}

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

    // Available thread
    std::vector<pthread_t> comp_threads(thread_num);

    size_t duration = 0;
    int count = 0;  // The number of iteration

    bool first = true;
    bool info = false;

    static std::chrono::high_resolution_clock::time_point begin, end;
    
    graphic::GraphicContext context{"Assignment 4"};
    context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {
        auto io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Assignment 4", nullptr,
                    ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoCollapse
                    | ImGuiWindowFlags_NoTitleBar
                    | ImGuiWindowFlags_NoResize);
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        ImGui::DragFloat("Block Size", &current_state.block_size, 0.01, 0.1, 10, "%f");

        if (first) {
            first = false;
            finished = false;
            begin = std::chrono::high_resolution_clock::now();
        }
        
        if (!finished) {

            /* Start calculation */
            auto beginCal = std::chrono::high_resolution_clock::now();

            current_thread = 0;
            finished = true;
            for (int i = 0; i < thread_num; i++) {
                pthread_create(&comp_threads[i], nullptr, thread_calculate, nullptr);
            }
            for (int i = 0; i < thread_num; i++) {
                pthread_join(comp_threads[i], nullptr);
            }

            grid.switch_buffer();

            auto endCal = std::chrono::high_resolution_clock::now();
            /* Finish calculation */

            count++;             
            duration += duration_cast<std::chrono::nanoseconds>(endCal - beginCal).count();
            if (count % ITERATION == 0) {
                std::cout << ITERATION << " elapse with " << duration << " nanoseconds\n";
                double speed = double(ITERATION) / double(duration) * 1e9;
                std::cout << "speed: " << speed << " iterations per second" << std::endl;
                duration = 0;
            }
            if (finished) end = std::chrono::high_resolution_clock::now();
        } else {
            long time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
            ImGui::Text("stabilized in %ld ns", time);
            if (!info) {
                std::cout << "stabilized in " << time << " ns" << std::endl;
                info = true;
            }
        }

        const ImVec2 p = ImGui::GetCursorScreenPos();
        float x = p.x + current_state.block_size, y = p.y + current_state.block_size;
        for (size_t i = 0; i < current_state.room_size; ++i) {
            for (size_t j = 0; j < current_state.room_size; ++j) {
                auto temp = grid[{i, j}];
                auto color = temp_to_color(temp);
                draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + current_state.block_size, y + current_state.block_size), color);
                y += current_state.block_size;
            }
            x += current_state.block_size;
            y = p.y + current_state.block_size;
        }
        ImGui::End();
    });        
    
    pthread_exit(NULL);
    return 0;
}
