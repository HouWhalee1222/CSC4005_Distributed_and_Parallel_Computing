#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <cstring>
#include <chrono>
#include <random>
#include <utility>
#include <iostream>
#include <stdio.h>

#include <cuda.h>
#include <cuda_runtime.h>


#define ITERATION 100
#define BODIES 200
#define COLLISION_RATIO 0.01

__device__ __managed__ double x[BODIES];
__device__ __managed__ double y[BODIES];
__device__ __managed__ double vx[BODIES];
__device__ __managed__ double vy[BODIES];
__device__ __managed__ double ax[BODIES];
__device__ __managed__ double ay[BODIES];
__device__ __managed__ double m[BODIES];

__device__ void handle_wall_collision(double position_range, double radius, int index) {
    bool flag = false;
    if (x[index] <= radius) {
        flag = true;
        x[index] = radius + radius * COLLISION_RATIO;
        vx[index] = -vx[index];
    } else if (x[index] >= position_range - radius) {
        flag = true;
        x[index] = position_range - radius - radius * COLLISION_RATIO;
        vx[index] = -vx[index];
    }

    if (y[index] <= radius) {
        flag = true;
        y[index] = radius + radius * COLLISION_RATIO;
        vy[index] = -vy[index];
    } else if (y[index] >= position_range - radius) {
        flag = true;
        y[index] = position_range - radius - radius * COLLISION_RATIO;
        vy[index] = -vy[index];
    }
    if (flag) {
        ax[index] = 0;
        ay[index] = 0;
    }
}

__device__  void update_for_tick(double elapse, double position_range, double radius, int index) {
    
    vx[index] += ax[index] * elapse;
    vy[index] += ay[index] * elapse;
    handle_wall_collision(position_range, radius, index);
    x[index] += vx[index] * elapse;
    y[index] += vy[index] * elapse;
    handle_wall_collision(position_range, radius, index);
}

__device__ void check_and_update(int i, int j, double radius, double gravity) {
    double delta_x = x[i] - x[j];
    double delta_y = y[i] - y[j];
    double distance_square = delta_x * delta_x + delta_y * delta_y;
    double ratio = 1 + COLLISION_RATIO;

    if (distance_square < radius * radius) {
        distance_square = radius * radius;
    }

    auto distance = std::sqrt(distance_square);

    if (distance < radius) {
        distance = radius;
    }

    if (distance_square <= radius * radius) {
        auto dot_prod = delta_x * (vx[i] - vx[j]) + delta_y * (vy[i] - vy[j]);
        auto scalar = 2 / (m[i] + m[j]) * dot_prod / distance_square;
        vx[i] -= scalar * delta_x * m[j];
        vy[i] -= scalar * delta_y * m[j];
        vx[j] += scalar * delta_x * m[i];
        vy[j] += scalar * delta_y * m[i];
        // now relax the distance a bit: after the collision, there must be
        // at least (ratio * radius) between them
        x[i] += delta_x / distance * ratio * radius / 2.0;
        y[i] += delta_y / distance * ratio * radius / 2.0;
        x[j] -= delta_x / distance * ratio * radius / 2.0;
        y[j] -= delta_y / distance * ratio * radius / 2.0;
    } else {
        // update acceleration only when no collision
        auto scalar = gravity / distance_square / distance;
        ax[i] -= scalar * delta_x * m[j];
        ay[i] -= scalar * delta_y * m[j];
        ax[j] += scalar * delta_x * m[i];
        ay[j] += scalar * delta_y * m[i];
    }
}

__global__ void update_for_tick(double elapse, double gravity, double position_range, double radius, int thread_num) {
    int rank = blockIdx.x * blockDim.x + threadIdx.x;
    int task = BODIES / thread_num;
    int start = rank * task;
    int end = start + task;
    if (rank == thread_num - 1) {
        end = BODIES;
    }
    // printf("Rank: %d\n", rank);

    for (int i = start; i < end; ++i) {
        for (int j = i + 1; j < BODIES; ++j) {
            if (j > start && j < start + task) {
                if (j <= i) continue;
            }
            check_and_update(i, j, radius, gravity);
        }
    }

    // Synchronize all the threads in this point
    __syncthreads();

    for (int i = start; i < end; ++i) {
        update_for_tick(elapse, position_range, radius, i);
    }


}

int main(int argc, char **argv) {
    int thread_num;

    if (argc == 1) {
        thread_num = 1;  // sequential by default
    } else if (argc == 2) {
        thread_num = atoi(*(argv + 1));
    } else {
        std::cerr << "usage: " << argv[0] << " <thread number> " << std::endl;
        return 0;
    }

    // Meta data
    static double gravity = 100;
    static double space = 800;
    static double radius = 5;
    // static int bodies = 200;
    static double elapse = 0.01;
    static ImVec4 color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
    static double max_mass = 50;

    // Initialize the data --- random
    std::random_device device;
    std::default_random_engine engine{device()};
    std::uniform_real_distribution<double> position_dist{0, space};
    std::uniform_real_distribution<double> mass_dist{0, max_mass};

    for (auto &i : x) {
        i = position_dist(engine);
    }
    for (auto &i : y) {
        i = position_dist(engine);
    }
    for (auto &i : m) {
        i = mass_dist(engine);
    }

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
        // ImGui::DragFloat("Space", &current_space, 10, 200, 1600, "%f");
        // ImGui::DragFloat("Gravity", &gravity, 0.5, 0, 1000, "%f");
        // ImGui::DragFloat("Radius", &radius, 0.5, 2, 20, "%f");
        // ImGui::DragInt("Bodies", &current_bodies, 1, 2, 100, "%d");
        // ImGui::DragFloat("Elapse", &elapse, 0.001, 0.001, 10, "%f");
        // ImGui::DragFloat("Max Mass", &current_max_mass, 0.5, 5, 100, "%f");
        // ImGui::ColorEdit4("Color", &color.x);

        {
            using namespace std::chrono;

            const ImVec2 p = ImGui::GetCursorScreenPos();

            /* Start calculation */
            auto begin = high_resolution_clock::now();           
            // Cuda initialization
            update_for_tick<<<1, thread_num>>>(elapse, gravity, space, radius, thread_num);
            
            // Wait for computation to finish
            cudaDeviceSynchronize();

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

            for (int i = 0; i < BODIES; ++i) {
                auto px = p.x + x[i];
                auto py = p.y + y[i];
                draw_list->AddCircleFilled(ImVec2(px, py), radius, ImColor{color});
            }
            
            for (int i = 0; i < BODIES; i++) {
                ax[i] = 0;
                ay[i] = 0;
            }
        }
        ImGui::End();
    });
    cudaDeviceReset();
    return 0;
}
