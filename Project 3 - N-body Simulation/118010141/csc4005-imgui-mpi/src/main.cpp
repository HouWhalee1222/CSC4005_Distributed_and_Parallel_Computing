#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <cstring>
#include <nbody/body.hpp>
#include <mpi.h>
#include <chrono>

// The number of iterations as a time duration
#define ITERATION 100  
#define MASTER 0

template <typename ...Args>
void UNUSED(Args&&... args [[maybe_unused]]) {}

int main(int argc, char **argv) {
    UNUSED(argc, argv);
    
    // MPI parmaeters
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

    // N-body simulation parameters
    static float gravity = 100;
    static float space = 800;
    static float radius = 5;
    static int bodies = 200;  // The body number may increase
    static float elapse = 0.01;
    static ImVec4 color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
    static float max_mass = 50;
    static float current_space = space;
    static float current_max_mass = max_mass;
    static int current_bodies = bodies;
    static int new_bodies = bodies;


    // Initialize a body pool in each process
    BodyPool pool(static_cast<size_t>(bodies), space, max_mass);
    pool.rank = rank; pool.proc_num = proc_num; pool.ori_size = bodies;

    if (rank == MASTER) {
        size_t duration = 0;
        int count = 0;  // The number of iteration

        graphic::GraphicContext context{"Assignment 3"};
        context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {
            {
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

                // Make bodies divisible by process number for allocation
                new_bodies = current_bodies;

                while (new_bodies % proc_num != 0) {
                    new_bodies++;
                }

                if (current_space != space || new_bodies != bodies || current_max_mass != max_mass) {
                    space = current_space;
                    bodies = new_bodies;
                    max_mass = current_max_mass;
                    pool = BodyPool{static_cast<size_t>(bodies), space, max_mass};
                    pool.rank = rank; pool.proc_num = proc_num; pool.ori_size = current_bodies;
                }

                {
                    using namespace std::chrono;
                    const ImVec2 p = ImGui::GetCursorScreenPos();

                    /* Start calculation */
                    auto begin = high_resolution_clock::now();
                    
                    // Broadcast meta data
                    MPI_Bcast(&gravity, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&space, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&radius, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&bodies, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&elapse, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&max_mass, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&current_bodies, 1, MPI_INT, 0, MPI_COMM_WORLD);

                    // Broadcast body data
                    MPI_Bcast(pool.x.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                    MPI_Bcast(pool.y.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                    MPI_Bcast(pool.vx.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                    MPI_Bcast(pool.vy.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                    MPI_Bcast(pool.m.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);

                    // Computation
                    pool.update_for_tick(elapse, gravity, space, radius);

                    // Receive results from other process
                    int task = bodies / proc_num;
                    MPI_Gather(&pool.x[rank * task], task, MPI_DOUBLE, pool.x.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
                    MPI_Gather(&pool.y[rank * task], task, MPI_DOUBLE, pool.y.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
                    MPI_Gather(&pool.vx[rank * task], task, MPI_DOUBLE, pool.vx.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
                    MPI_Gather(&pool.vy[rank * task], task, MPI_DOUBLE, pool.vy.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);

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

                    for (size_t i = 0; i < size_t(current_bodies); ++i) {
                        auto body = pool.get_body(i);
                        auto x = p.x + static_cast<float>(body.get_x());
                        auto y = p.y + static_cast<float>(body.get_y());
                        draw_list->AddCircleFilled(ImVec2(x, y), radius, ImColor{color});
                    }
                }
                ImGui::End();
            }
        });
    } else {  // Salve process
        int prev_bodies = bodies;

        while (true) {  // run repeatedly
            // Receive meta data
            MPI_Bcast(&gravity, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&space, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&radius, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&bodies, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&elapse, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&max_mass, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&current_bodies, 1, MPI_INT, 0, MPI_COMM_WORLD);

            if (prev_bodies != bodies || current_space != space || current_max_mass != max_mass) {
                // Initialize a new empty pool
                pool = BodyPool{static_cast<size_t>(bodies), space, max_mass};
                pool.rank = rank; pool.proc_num = proc_num; pool.ori_size = current_bodies;
            }

            prev_bodies = bodies;
            current_space = space;
            current_max_mass = max_mass;

            // Receive body data
            MPI_Bcast(pool.x.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Bcast(pool.y.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Bcast(pool.vx.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Bcast(pool.vy.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Bcast(pool.m.data(), bodies, MPI_DOUBLE, 0, MPI_COMM_WORLD);

            // Computation
            pool.update_for_tick(elapse, gravity, space, radius);

            // Send back the result
            int task = bodies / proc_num;
            MPI_Gather(&pool.x[rank * task], task, MPI_DOUBLE, pool.x.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
            MPI_Gather(&pool.y[rank * task], task, MPI_DOUBLE, pool.y.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
            MPI_Gather(&pool.vx[rank * task], task, MPI_DOUBLE, pool.vx.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
            MPI_Gather(&pool.vy[rank * task], task, MPI_DOUBLE, pool.vy.data(), task, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}
