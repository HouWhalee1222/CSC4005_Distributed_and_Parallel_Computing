#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <cstring>
#include <chrono>
#include <iostream>
#include <hdist/hdist.hpp>

#include <mpi.h>

#define MASTER 0
#define ITERATION 10

template<typename ...Args>
void UNUSED(Args &&... args [[maybe_unused]]) {}

ImColor temp_to_color(double temp) {
    auto value = static_cast<uint8_t>(temp / 100.0 * 255.0);
    return {value, 0, 255 - value};
}

int main(int argc, char **argv) {
    UNUSED(argc, argv);

    // MPI parameters
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

    // Initialize the grid in each process
    static hdist::State current_state;

    auto grid = hdist::Grid{
            static_cast<size_t>(current_state.room_size),
            current_state.border_temp,
            current_state.source_temp,
            static_cast<size_t>(current_state.source_x),
            static_cast<size_t>(current_state.source_y),
            rank,
            proc_num};

    if (rank == MASTER) {
        size_t duration = 0;
        int count = 0;  // The number of iteration

        bool first = true;
        bool finished = false;
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

                // Broadcast grid
                MPI_Bcast(grid.get_current_buffer().data(), grid.length * grid.length, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                
                // Computation
                finished = hdist::calculate(current_state, grid);

                // Receive results from other process
                int task = grid.length / grid.proc_num;  // The number of columns 
                int remain = grid.length % grid.proc_num;  // extra columns

                MPI_Gather(&grid[{rank * task + remain, 0}], task * grid.length, MPI_DOUBLE, &grid[{remain, 0}], task * grid.length, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);

                bool finishedSlave = false;
                for (int p = 1; p < proc_num; p++) {
                    MPI_Recv(&finishedSlave, 1, MPI_CHAR, p, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    finished &= finishedSlave;
                }

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
                    std::cout << "stabilized in " << time << " ns and " << count << " iterations" << std::endl;
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
        
    } else {
        bool finished = false;
        while (true) {  // run repeatedly
            // Receive grid data
            MPI_Bcast(grid.get_current_buffer().data(), grid.length * grid.length, MPI_DOUBLE, 0, MPI_COMM_WORLD);

            // Computation
            finished = hdist::calculate(current_state, grid);
        
            // Send back the result
            int task = grid.length / grid.proc_num;  // The number of columns 
            int remain = grid.length % grid.proc_num;  // extra columns
            
            MPI_Gather(&grid[{rank * task + remain, 0}], task * grid.length, MPI_DOUBLE, &grid[{remain, 0}], task * grid.length, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
            MPI_Send(&finished, 1, MPI_CHAR, MASTER, MASTER, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}
