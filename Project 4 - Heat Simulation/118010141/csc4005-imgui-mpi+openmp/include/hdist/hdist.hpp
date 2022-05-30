#pragma once

#include <vector>
#include <cmath>
#include <iostream>

#include <omp.h>

namespace hdist {

    enum class Algorithm : int {
        Jacobi = 0,
        Sor = 1
    };

    struct State {
        int room_size = 300;
        float block_size = 2;
        int source_x = room_size / 2;
        int source_y = room_size / 2;
        float source_temp = 100;
        float border_temp = 36;
        float tolerance = 0.1;
        float sor_constant = 4.0;
        Algorithm algo = hdist::Algorithm::Jacobi;

        bool operator==(const State &that) const = default;
    };

    struct Alt {
    };

    constexpr static inline Alt alt{};

    struct Grid {
        std::vector<double> data0, data1;
        size_t current_buffer = 0;
        size_t length;
        int rank;
        int proc_num;

        explicit Grid(size_t size,
                      double border_temp,
                      double source_temp,
                      size_t x,
                      size_t y,
                      int r,
                      int p)
                : data0(size * size), data1(size * size), length(size), rank(r), proc_num(p) {
            for (size_t i = 0; i < length; ++i) {
                for (size_t j = 0; j < length; ++j) {
                    if (i == 0 || j == 0 || i == length - 1 || j == length - 1) {
                        this->operator[]({i, j}) = border_temp;
                    } else if (i == x && j == y) {
                        this->operator[]({i, j}) = source_temp;
                    } else {
                        this->operator[]({i, j}) = 0;
                    }
                }
            }
        }

        std::vector<double> &get_current_buffer() {
            if (current_buffer == 0) return data0;
            return data1;
        }

        double &operator[](std::pair<size_t, size_t> index) {
            return get_current_buffer()[index.first * length + index.second];
        }

        double &operator[](std::tuple<Alt, size_t, size_t> index) {
            return current_buffer == 1 ? data0[std::get<1>(index) * length + std::get<2>(index)] : data1[
                    std::get<1>(index) * length + std::get<2>(index)];
        }

        void switch_buffer() {
            current_buffer = !current_buffer;
        }
    };

    struct UpdateResult {
        bool stable;
        double temp;
    };

    UpdateResult update_single(size_t i, size_t j, Grid &grid, const State &state) {
        UpdateResult result{};
        if (i == 0 || j == 0 || i == state.room_size - 1 || j == state.room_size - 1) {
            result.temp = state.border_temp;
        } else if (i == state.source_x && j == state.source_y) {
            result.temp = state.source_temp;
        } else {
            auto sum = (grid[{i + 1, j}] + grid[{i - 1, j}] + grid[{i, j + 1}] + grid[{i, j - 1}]);
            switch (state.algo) {
                case Algorithm::Jacobi:
                    result.temp = 0.25 * sum;
                    break;
                case Algorithm::Sor:
                    result.temp = grid[{i, j}] + (1.0 / state.sor_constant) * (sum - 4.0 * grid[{i, j}]);
                    break;
            }
        }
        result.stable = std::fabs(grid[{i, j}] - result.temp) < state.tolerance;
        // if (!result.stable) std::cout << grid[{i, j}] << " " << result.temp << " Position: " << i << " " << j << std::endl;
        return result;
    }

    bool calculate(const State &state, Grid &grid) {    
        bool stabilized = true;
        int task = grid.length / grid.proc_num;
        int remain = grid.length % grid.proc_num;  // extra columns
        size_t start = grid.rank * task + remain;
        size_t end = start + task;

        if (grid.rank == 0) {
            start = 0;
        }
        
        size_t i, j;
        switch (state.algo) {
            case Algorithm::Jacobi:
                for (i = start; i < end; ++i) {
                    #pragma omp parallel for
                    for (j = 0; j < state.room_size; ++j) {
                        auto result = update_single(i, j, grid, state);
                        stabilized &= result.stable;
                        grid[{alt, i, j}] = result.temp;
                    }
                }
                grid.switch_buffer();
                break;
            case Algorithm::Sor:
                for (auto k : {0, 1}) {
                    for (i = start; i < end; ++i) {
                        #pragma omp parallel for
                        for (j = 0; j < state.room_size; ++j) {
                            if (k == ((i + j) & 1)) {
                                auto result = update_single(i, j, grid, state);
                                stabilized &= result.stable;
                                grid[{alt, i, j}] = result.temp;
                            } else {
                                grid[{alt, i, j}] = grid[{i, j}];
                            }
                        }
                    }
                    grid.switch_buffer();
                }
        }

        return stabilized;
    };


} // namespace hdist