#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <cstring>
#include <chrono>
#include <iostream>
#include <cmath>
#include <hdist/hdist.hpp>
#include <stdio.h>

#include <cuda.h>
#include <cuda_runtime.h>

#define ITERATION 10

// Parameters
#define ROOMSIZE 300
#define SOURCEX 150
#define SOURCEY 150
#define SOURCETEMP 100
#define BOARDTEMP 36
#define TOLERANCE 0.1

__device__ __managed__ double data0[ROOMSIZE * ROOMSIZE];
__device__ __managed__ double data1[ROOMSIZE * ROOMSIZE];
__device__ __managed__ bool finished = false;
__device__ __managed__ int thread_num = 1;
__device__ __managed__ bool first_buffer = true;

__device__ double update_single(int i, int j) {
    double temp = 0.0;
    if (i == 0 || j == 0 || i == ROOMSIZE - 1 || j == ROOMSIZE - 1) {
        temp = double(BOARDTEMP);
    } else if (i == SOURCEX && j == SOURCEY) {
        temp = double(SOURCETEMP);
    } else {
        bool stable = false;
        double* data = (first_buffer) ? data0 : data1;
        double sum = data[(i + 1) * ROOMSIZE + j] + data[(i - 1) * ROOMSIZE + j] + data[i * ROOMSIZE + (j + 1)] + data[i * ROOMSIZE + (j - 1)];
        temp = 0.25 * sum;
        stable = std::fabs(data[i * ROOMSIZE + j] - temp) < TOLERANCE;
        finished &= stable;
        // if (temp != 0.0) printf("i: %d j: %d, temp: %lf up:%lf down: %lf left:%lf right:%lf \n", i, j, temp, data[(i + 1) * ROOMSIZE + j], data[(i - 1) * ROOMSIZE + j], data[i * ROOMSIZE + (j + 1)], data[i * ROOMSIZE + (j - 1)]);
    }
    return temp;
}

__global__ void calculate() {  
    int rank = blockIdx.x * blockDim.x + threadIdx.x;
    int task = int(ROOMSIZE) / thread_num;
    int remain = int(ROOMSIZE) % thread_num;    
    int start = rank * task + remain;
    int end = start + task;

    double* writeData = (first_buffer) ? data1 : data0; 

    if (rank == 0) {
        start = 0;
    }

    // printf("rank: %d start: %d end: %d task: %d", rank, int(start), int(end), task);

    for (int i = start; i < end; ++i) {
        for (int j = 0; j < ROOMSIZE; ++j) {
            double temp = update_single(i, j);
            writeData[i * ROOMSIZE + j] = temp;
        }
    }
}

__host__ ImColor temp_to_color(double temp) {
    auto value = static_cast<uint8_t>(temp / 100.0 * 255.0);
    return {value, 0, 255 - value};
}

__host__ void init_data() {
    for (int i = 0; i < ROOMSIZE; ++i) {
        for (int j = 0; j < ROOMSIZE; ++j) {
            if (i == 0 || j == 0 || i == ROOMSIZE - 1 || j == ROOMSIZE - 1) {
                data0[i * ROOMSIZE + j] = BOARDTEMP;
            } else if (i == SOURCEX && j == SOURCEY) {
                data0[i * ROOMSIZE + j] = SOURCETEMP;
            } else {
                data0[i * ROOMSIZE + j] = 0;
            }
        }
    }
}

int main(int argc, char **argv) {

    // Initialize the tempature
    init_data();  // strange bug, after init_data, thread_num becomes zero
    
    // Print the buffer data
    // for (int i = 0; i < ROOMSIZE; ++i) {
    //     for (int j = 0; j < ROOMSIZE; ++j) {
    //         printf("%lf ", data0[i * ROOMSIZE + j]);
    //     }
    //     printf("\n");
    // }
    
    if (argc == 1) {
        thread_num = 1;  // sequential by default
    } else if (argc == 2) {
        thread_num = atoi(*(argv + 1));
    } else {
        std::cerr << "usage: " << argv[0] << " <thread number> " << std::endl;
        return 0;
    }

    // Decide the graph size
    static float block_size = 2;
    int duration = 0;
    int count = 0;  // The number of iteration

    bool first = true;
    bool info = false;

    static std::chrono::high_resolution_clock::time_point begin, end;
    graphic::GraphicContext context{"Assignment 4"};
    context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {
        using namespace std::chrono;
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
        ImGui::DragFloat("Block Size", &block_size, 0.01, 0.1, 10, "%f");

        if (first) {
            first = false;
            finished = false;
            begin = std::chrono::high_resolution_clock::now();
        }

        if (!finished) {

            /* Start calculation */
            auto beginCal = high_resolution_clock::now();
            finished = true;

            // Computation
            calculate<<<1, thread_num>>>();

            // Wait for computation to finish
            cudaDeviceSynchronize();
            
            first_buffer = !first_buffer;
            auto endCal = high_resolution_clock::now();
            /* Finish calculation */
            count++;                
            duration += duration_cast<nanoseconds>(endCal - beginCal).count();
            if (count % ITERATION == 0) {
                std::cout << ITERATION << " elapse with " << duration << " nanoseconds\n";
                double speed = double(ITERATION) / double(duration) * 1e9;
                std::cout << "speed: " << speed << " iterations per second" << std::endl;
                duration = 0;
            }

            if (finished) end = high_resolution_clock::now();
        
        } else {
            long time = duration_cast<nanoseconds>(end - begin).count();
            ImGui::Text("stabilized in %ld ns", time);
            if (!info) {
                std::cout << "stabilized in " << time << " ns and " << count << " iterations" << std::endl;
                info = true;
            }
        }

        const ImVec2 p = ImGui::GetCursorScreenPos();
        double* data = (first_buffer) ? data0 : data1;
        float x = p.x + block_size, y = p.y + block_size;
        for (int i = 0; i < ROOMSIZE; ++i) {
            for (int j = 0; j < ROOMSIZE; ++j) {
                double temp = data[i * ROOMSIZE + j];
                auto color = temp_to_color(temp);
                draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + block_size, y + block_size), color);
                y += block_size;
            }
            x += block_size;
            y = p.y + block_size;
        }
        ImGui::End();
    });        
    cudaDeviceReset();
    return 0;
}
