#include <gtest/gtest.h>
#include <odd-even-sort.hpp>
#include <random>
#include <mpi.h>

using namespace sort;
std::unique_ptr<Context> context;
TEST(OddEvenSort, Basic) {

    int rank;
    uint64_t cases;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        std::vector<std::vector<Element>> data{
                {-1},
                {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
                {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3},
                {1, 43, 1245, 41235246, 12, 123123, -123, 0},
                std::vector<Element>(1024, 123)
        };
        cases = data.size();
        MPI_Bcast(&cases, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        for (auto &i : data) {
            std::vector<Element> a = i;
            std::vector<Element> b = i;
            context->mpi_sort(a.data(), a.data() + a.size());
            std::sort(b.data(), b.data() + b.size());
            EXPECT_EQ(a, b);
        }
    } else {
        MPI_Bcast(&cases, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        for (uint64_t i = 0; i < cases; ++i) {
            context->mpi_sort(nullptr, nullptr);
        }
    }
}

TEST(OddEvenSort, Random) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        std::vector<Element> data(48'000);
        auto dev = std::random_device{};
        auto seed = dev();
        auto gen = std::default_random_engine(seed);
        auto dist = std::uniform_int_distribution<Element>{};
        for (auto &i : data) {
            i = dist(gen);
        }
        std::vector<Element> a = data;
        std::vector<Element> b = data;
        context->mpi_sort(a.data(), a.data() + a.size());
        std::sort(b.data(), b.data() + b.size());
        EXPECT_EQ(a, b) << " seeded with: " << seed << std::endl;
    } else {
        context->mpi_sort(nullptr, nullptr);
    }
}

int main(int argc, char **argv) {
    context = std::make_unique<Context>(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::TestEventListeners &listeners =
            ::testing::UnitTest::GetInstance()->listeners();
    if (rank != 0) {
        delete listeners.Release(listeners.default_result_printer());
    }
    auto res = RUN_ALL_TESTS();
    context.reset();
    return res;
}
