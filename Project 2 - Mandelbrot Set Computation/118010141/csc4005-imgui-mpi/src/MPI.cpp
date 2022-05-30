#include <chrono>
#include <iostream>
#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <vector>
#include <complex>
#include <mpi.h>
#include <cstring>

#define MASTER 0

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

    int* pointer() {
        return buffer.data();
    }
};

void calculate(int* local, int* remain, int rank, int proc_num, int size, double scale, double x_center, double y_center, int k_value) {
    
    double cx = static_cast<double>(size) / 2 + x_center;
    double cy = static_cast<double>(size) / 2 + y_center;
    double zoom_factor = static_cast<double>(size) / 4 * scale;
    int base = 0;

    for (int i = rank; i < size; i += proc_num) {  // distribute row by row
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
            if (i < size / proc_num * proc_num) {
                local[base + j] = k;
            } 
            else {
                remain[j] = k;
            } 
        }
        base += size;
    }
}

int main(int argc, char **argv) {
    int res;
    int rank;
    int proc_num;

    MPI_Init(&argc, &argv);

    res = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (MPI_SUCCESS != res) {
        throw std::runtime_error("failed to get MPI world rank");
    }
    res = MPI_Comm_size(MPI_COMM_WORLD, &proc_num);
    if (MPI_SUCCESS != res) {
        throw std::runtime_error("failed to get MPI world size");
    }

    // Local buffer
    int* local;
    int* remain;

    // Total buffer
    Square canvas(100);

    if (rank == MASTER) {
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

                // Parameter initialization in master process
                static int center_x = 0;
                static int center_y = 0;
                static int size = 800;
                static double scale = 0.5;
                static ImVec4 col = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                static int k_value = 100;

                ImGui::DragInt("Center X", &center_x, 1, -4 * size, 4 * size, "%d");
                ImGui::DragInt("Center Y", &center_y, 1, -4 * size, 4 * size, "%d");
                ImGui::DragInt("Fineness", &size, 10, 100, 1600, "%d");
                // ImGui::DragInt("Scale", &scale, 1, 10, 100, "%.01f"); // 10?
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
                    MPI_Bcast(&center_x, 1, MPI_INT, 0, MPI_COMM_WORLD);  // Broadcast all the meta parameters
                    MPI_Bcast(&center_y, 1, MPI_INT, 0, MPI_COMM_WORLD); 
                    MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD); 
                    MPI_Bcast(&scale, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); 
                    MPI_Bcast(&k_value, 1, MPI_INT, 0, MPI_COMM_WORLD); 

                    local = (int*)malloc(size * (size / proc_num) * sizeof(int));  // allocate local buffer
                    remain = (int*)malloc(size * sizeof(int));

                    calculate(local, remain, rank, proc_num, size, scale, center_x, center_y, k_value);
                    MPI_Gather(local, size * (size / proc_num), MPI_INT, canvas.pointer(), size * (size / proc_num), MPI_INT, MASTER, MPI_COMM_WORLD);
                    
                    // Copy the remaining line in master process
                    if (size % proc_num != 0) {
                        for (int i = 0; i < size; i++) {
                            *(canvas.pointer() + size * (size / proc_num) * proc_num + i) = *(remain + i);
                        }
                    }

                    // Copy the remaining line in slave process
                    for (int i = 1; i < size % proc_num; i++) {
                        MPI_Recv(canvas.pointer() + size * ((size / proc_num) * proc_num + i), size, MPI_INT, i, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    }
                    
                    free(local);
                    free(remain);
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
                    for (int i = 0; i < size; i++) {
                        int i_new;
                        if (i < size / proc_num * proc_num) {
                            i_new = i % proc_num * (size / proc_num) + i / proc_num;
                        } else {  // the remaining part is in order
                            i_new = i;
                        }
                        
                        for (int j = 0; j < size; j++) {
                            if (canvas[{i_new, j}] == k_value) {
                                draw_list->AddCircleFilled(ImVec2(x, y), radius, col32);
                                // std::cout << i << " " << j << std::endl;
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
    } else {  // Slave process calculation
        int center_x;
        int center_y;
        int size;
        double scale;
        ImVec4 col;
        int k_value;

        while (true) {  // run repeatedly
            MPI_Bcast(&center_x, 1, MPI_INT, 0, MPI_COMM_WORLD);  // Broadcast all the meta parameters
            MPI_Bcast(&center_y, 1, MPI_INT, 0, MPI_COMM_WORLD); 
            MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD); 
            MPI_Bcast(&scale, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); 
            MPI_Bcast(&k_value, 1, MPI_INT, 0, MPI_COMM_WORLD); 

            local = (int*)malloc(size * (size / proc_num) * sizeof(int));  // allocate local buffer
            remain = (int*)malloc(size * sizeof(int));
            
            calculate(local, remain, rank, proc_num, size, scale, center_x, center_y, k_value);
            MPI_Gather(local, size * (size / proc_num), MPI_INT, canvas.pointer(), size * (size / proc_num), MPI_INT, MASTER, MPI_COMM_WORLD);
            if (rank < size % proc_num) {
                MPI_Send(remain, size, MPI_INT, 0, MASTER, MPI_COMM_WORLD);
            }

            free(local);
            free(remain);
        }
    }

    MPI_Finalize();
    return 0;
}
