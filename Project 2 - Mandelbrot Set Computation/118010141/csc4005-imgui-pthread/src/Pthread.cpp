#include <chrono>
#include <iostream>
#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <vector>
#include <complex>
#include <pthread.h>
#include <cstring>
#include <atomic>

static constexpr float MARGIN = 4.0f;
static constexpr float BASE_SPACING = 2000.0f;
static constexpr size_t SHOW_THRESHOLD = 500000000ULL;

struct Square {
    std::vector<int> buffer;
    size_t length;

    explicit Square(size_t length) : buffer(length), length(length * length) {}

    void resize(size_t new_length) {
        buffer.assign(new_length * new_length, false);
        length = new_length;
    }

    auto& operator[](std::pair<size_t, size_t> pos) {
        return buffer[pos.first * length + pos.second];  // modify
    }
} canvas(100);  // Total buffer, global

// Parameter initialization as global variables
int center_x = 0;
int center_y = 0;
int size = 800;
double scale = 0.5;
ImVec4 col = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
int k_value = 100;

// Thread variable
int thread_num;
std::atomic_int current_thread{0};

void *calculate(void *) {
    int rank = current_thread;
    current_thread++;

    double cx = static_cast<double>(size) / 2 + center_x;
    double cy = static_cast<double>(size) / 2 + center_y;
    double zoom_factor = static_cast<double>(size) / 4 * scale;

    for (int i = rank; i < size; i += thread_num) {  // distribute row by row
        for (int j = 0; j < size; j++) {
            double x = (static_cast<double>(j) - cx) / zoom_factor;
            double y = (static_cast<double>(i) - cy) / zoom_factor;
            std::complex<double> z{0, 0};
            std::complex<double> c{x, y};
            int k = 0;
            do {
                z = z * z + c;
                k++;
            } while (norm(z) < 2.0 && k < k_value);  // only paint the points iterting k_value times
            canvas[{i, j}] = k;
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv) {

    if (argc == 1) {
        thread_num = 1;  // sequential by default
    } else if (argc == 2) {
        thread_num = atoi(*(argv + 1));
    } else {
        std::cerr << "usage: " << argv[0] << " <thread number> " << std::endl;
        return 0;
    }

    // Available thread
    std::vector<pthread_t> cal_thread(thread_num);

    graphic::GraphicContext context{"Assignment 2"};
    size_t duration = 0;
    size_t pixels = 0;
    context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {
        {
            auto io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Assignment 2", nullptr,
                            ImGuiWindowFlags_NoMove
                            | ImGuiWindowFlags_NoCollapse
                            | ImGuiWindowFlags_NoTitleBar
                            | ImGuiWindowFlags_NoResize);
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);

            ImGui::DragInt("Center X", &center_x, 1, -4 * size, 4 * size, "%d");
            ImGui::DragInt("Center Y", &center_y, 1, -4 * size, 4 * size, "%d");
            ImGui::DragInt("Fineness", &size, 10, 100, 1600, "%d");
            // ImGui::DragInt("Scale", &scale, 1, 1, 100, "%.01f");
            ImGui::DragInt("K", &k_value, 1, 100, 1000, "%d");
            ImGui::ColorEdit4("Color", &col.x);

            {
                using namespace std::chrono;
                auto spacing = BASE_SPACING / static_cast<float>(size);  // The larger the size, the smaller the spacing
                auto radius = spacing / 2;
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const ImU32 col32 = ImColor(col);
                float x = p.x + MARGIN, y = p.y + MARGIN;
                canvas.resize(size);

                /* Start calculation */
                auto begin = high_resolution_clock::now();
                current_thread = 0;
                for (int i = 0; i < thread_num; i++) {
                    pthread_create(&cal_thread[i], nullptr, calculate, nullptr);
                }
                for (int i = 0; i < thread_num; i++) {
                    pthread_join(cal_thread[i], nullptr);
                }
 
                auto end = high_resolution_clock::now();
                /* Finish calculation */

                pixels += size;
                duration += duration_cast<nanoseconds>(end - begin).count();

                if (duration > SHOW_THRESHOLD) {
                    std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
                    auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
                    std::cout << "speed: " << speed << " pixels per second" << std::endl;
                    pixels = 0;
                    duration = 0;
                }

                // Because the point allocation way is different, the drawing procedure is required to be modified
                for (int i = 0; i < size; ++i) {
                    for (int j = 0; j < size; ++j) {
                        if (canvas[{i, j}] == k_value) {
                            draw_list->AddCircleFilled(ImVec2(x, y), radius, col32);
                        }
                        x += spacing;
                    }
                    y += spacing;
                    x = p.x + MARGIN;
                }
            }
            ImGui::End();
        }
    });

    pthread_exit(NULL);
    return 0;
}
