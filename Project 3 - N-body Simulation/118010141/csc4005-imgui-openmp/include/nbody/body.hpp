//
// Created by schrodinger on 11/2/21.
//
#pragma once

#include <random>
#include <utility>
#include <omp.h>
#include <vector>

// The number of threads
int thread_num;

class BodyPool {
public:
    // provides in this way so that
    // it is easier for you to send a the vector with MPI
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> vx;
    std::vector<double> vy;
    std::vector<double> ax;
    std::vector<double> ay;
    std::vector<double> m;
    // so the movements of bodies are calculated discretely.
    // if after the collision, we do not separate the bodies a little bit, it may
    // results in strange outcomes like infinite acceleration.
    // hence, we will need to set up a ratio for separation.
    static constexpr double COLLISION_RATIO = 0.01;


    class Body {
        size_t index;
        BodyPool &pool;

        friend class BodyPool;

        Body(size_t index, BodyPool &pool) : index(index), pool(pool) {}

    public:
        double &get_x() {
            return pool.x[index];
        }

        double &get_y() {
            return pool.y[index];
        }

        double &get_vx() {
            return pool.vx[index];
        }

        double &get_vy() {
            return pool.vy[index];
        }

        double &get_ax() {
            return pool.ax[index];
        }

        double &get_ay() {
            return pool.ay[index];
        }

        double &get_m() {
            return pool.m[index];
        }

        double distance_square(Body &that) {
            auto delta_x = get_x() - that.get_x();
            auto delta_y = get_y() - that.get_y();
            return delta_x * delta_x + delta_y * delta_y;
        }

        double distance(Body &that) {
            return std::sqrt(distance_square(that));
        }

        double delta_x(Body &that) {
            return get_x() - that.get_x();
        }

        double delta_y(Body &that) {
            return get_y() - that.get_y();
        }

        bool collide(Body &that, double radius) {
            return distance_square(that) <= radius * radius;
        }

        // collision with wall
        void handle_wall_collision(double position_range, double radius, std::vector<double>& new_x, std::vector<double>& new_y, std::vector<double>& new_vx, std::vector<double>& new_vy, std::vector<double>& new_ax, std::vector<double>& new_ay) {
            bool flag = false;
            if (new_x[index] <= radius) {
                flag = true;
                new_x[index] = radius + radius * COLLISION_RATIO;
                new_vx[index] = -new_vx[index];
            } else if (new_x[index] >= position_range - radius) {
                flag = true;
                new_x[index] = position_range - radius - radius * COLLISION_RATIO;
                new_vx[index] = -new_vx[index];
            }

            if (new_y[index] <= radius) {
                flag = true;
                new_y[index] = radius + radius * COLLISION_RATIO;
                new_vy[index] = -new_vy[index];
            } else if (new_y[index] >= position_range - radius) {
                flag = true;
                new_y[index] = position_range - radius - radius * COLLISION_RATIO;
                new_vy[index] = -new_vy[index];
            }

            if (flag) {
                new_ax[index] = 0;
                new_ay[index] = 0;
            }
        }

        void update_for_tick(double elapse, double position_range, double radius, std::vector<double>& new_x, std::vector<double>& new_y, std::vector<double>& new_vx, std::vector<double>& new_vy, std::vector<double>& new_ax, std::vector<double>& new_ay) {
            new_vx[index] += new_ax[index] * elapse;
            new_vy[index] += new_ay[index] * elapse;

            handle_wall_collision(position_range, radius, new_x, new_y, new_vx, new_vy, new_ax, new_ay);

            new_x[index] += new_vx[index] * elapse;
            new_y[index] += new_vy[index] * elapse;

            handle_wall_collision(position_range, radius, new_x, new_y, new_vx, new_vy, new_ax, new_ay);
        }

    };

    BodyPool(size_t size, double position_range, double mass_range) :
            x(size), y(size), vx(size), vy(size), ax(size), ay(size), m(size) {
        std::random_device device;
        std::default_random_engine engine{device()};
        std::uniform_real_distribution<double> position_dist{0, position_range};
        std::uniform_real_distribution<double> mass_dist{0, mass_range};
        for (auto &i : x) {
            i = position_dist(engine);
        }
        for (auto &i : y) {
            i = position_dist(engine);
        }
        for (auto &i : m) {
            i = mass_dist(engine);
        }
    }

    Body get_body(size_t index) {
        return {index, *this};
    }

    void clear_acceleration() {
        ax.assign(m.size(), 0.0);
        ay.assign(m.size(), 0.0);
    }

    size_t size() {
        return m.size();
    }

    static void check_and_update(Body i, Body j, double radius, double gravity, std::vector<double>& new_x, std::vector<double>& new_y, std::vector<double>& new_vx, std::vector<double>& new_vy, std::vector<double>& new_ax, std::vector<double>& new_ay) {
        auto delta_x = i.delta_x(j);
        auto delta_y = i.delta_y(j);
        auto distance_square = i.distance_square(j);
        auto ratio = 1 + COLLISION_RATIO;
        if (distance_square < radius * radius) {
            distance_square = radius * radius;
        }
        auto distance = i.distance(j);
        if (distance < radius) {
            distance = radius;
        }
        if (i.collide(j, radius)) {
            auto dot_prod = delta_x * (i.get_vx() - j.get_vx())
                            + delta_y * (i.get_vy() - j.get_vy());
            auto scalar = 2 / (i.get_m() + j.get_m()) * dot_prod / distance_square;

            new_vx[i.index] -= scalar * delta_x * j.get_m();
            new_vy[i.index] -= scalar * delta_y * j.get_m();

            new_vx[j.index] += scalar * delta_x * i.get_m();
            new_vy[j.index] += scalar * delta_y * i.get_m();

            
            // now relax the distance a bit: after the collision, there must be
            // at least (ratio * radius) between them

            new_x[i.index] += delta_x / distance * ratio * radius / 2.0;
            new_y[i.index] += delta_y / distance * ratio * radius / 2.0;

            new_x[j.index] -= delta_x / distance * ratio * radius / 2.0;
            new_y[j.index] -= delta_y / distance * ratio * radius / 2.0;
        } else {
            // update acceleration only when no collision
            auto scalar = gravity / distance_square / distance;
            new_ax[i.index] -= scalar * delta_x * j.get_m();
            new_ay[i.index] -= scalar * delta_y * j.get_m();

            new_ax[j.index] += scalar * delta_x * i.get_m();
            new_ay[j.index] += scalar * delta_y * i.get_m();
        }
    }

    void update_for_tick(double elapse,
                         double gravity,
                         double position_range,
                         double radius) {
        ax.assign(size(), 0);
        ay.assign(size(), 0);

        // Local variable to write
        std::vector<double> new_x(x);
        std::vector<double> new_y(y);
        std::vector<double> new_vx(vx);
        std::vector<double> new_vy(vy);
        std::vector<double> new_ax(ax);
        std::vector<double> new_ay(ay);

        // Temporary vectors to avoid data race
        std::vector<double> temp_x(size());
        std::vector<double> temp_y(size());
        std::vector<double> temp_vx(size());
        std::vector<double> temp_vy(size());
        std::vector<double> temp_ax(size());
        std::vector<double> temp_ay(size());

        size_t task = size() / thread_num;
        size_t start = size() + 1;

        size_t i, j;

        #pragma omp parallel for private(j) firstprivate(start, new_x, new_y, new_vx, new_vy, new_ax, new_ay)
        for (i = 0; i < size(); ++i) {
            if (start == size() + 1) {
                start = i;
            }
            for (j = 0; j < size(); ++j) {
                if (j > start && j < start + task) {
                    if (j <= i) continue;
                }   
                check_and_update(get_body(i), get_body(j), radius, gravity, new_x, new_y, new_vx, new_vy, new_ax, new_ay);             
            }

            temp_x[i] = new_x[i]; temp_y[i] = new_y[i];
            temp_vx[i] = new_vx[i]; temp_vy[i] = new_vy[i];
            temp_ax[i] = new_ax[i]; temp_ay[i] = new_ay[i];
        }

        new_x.assign(temp_x.begin(), temp_x.end()); new_y.assign(temp_y.begin(), temp_y.end());
        new_vx.assign(temp_vx.begin(), temp_vx.end()); new_vy.assign(temp_vy.begin(), temp_vy.end());
        new_ax.assign(temp_ax.begin(), temp_ax.end()); new_ay.assign(temp_ay.begin(), temp_ay.end());

        #pragma omp parallel for firstprivate(new_x, new_y, new_vx, new_vy, new_ax, new_ay)
        for (i = 0; i < size(); ++i) {
            get_body(i).update_for_tick(elapse, position_range, radius, new_x, new_y, new_vx, new_vy, new_ax, new_ay);
            
            temp_x[i] = new_x[i]; temp_y[i] = new_y[i];
            temp_vx[i] = new_vx[i]; temp_vy[i] = new_vy[i];
        }

        x.assign(temp_x.begin(), temp_x.end()); y.assign(temp_y.begin(), temp_y.end());
        vx.assign(temp_vx.begin(), temp_vx.end()); vy.assign(temp_vy.begin(), temp_vy.end());
    }
};

