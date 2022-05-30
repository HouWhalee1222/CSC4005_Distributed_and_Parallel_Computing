#include <odd-even-sort.hpp>
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    sort::Context context(argc, argv);
    int rank;
    int size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            std::cerr << "wrong arguments" << std::endl;
            std::cerr << "usage: " << argv[0] << " <input-file> <output-file>" << std::endl;
        }
        return 0;
    }

    if (rank == 0) {
        sort::Element element;
        std::vector<sort::Element> data;
        std::ifstream input(argv[1]);
        std::ofstream output(argv[2]);
        while (input >> element) {
            data.push_back(element);
        }
        int dataSize = data.size();
        if (dataSize < size) {  // data < process number 
            for (int i = 0; i < (size - dataSize); i++) {
                data.push_back(RAND_MAX);
            }
        }

        auto info = context.mpi_sort(data.data(), data.data() + data.size());
        info->length = dataSize;
        sort::Context::print_information(*info, std::cout);
        int count = 1;
        for (int i = 0; i < dataSize; i++) {
            output << data[i] << " ";
            if (count++ % 20 == 0) {
                output << std::endl;
            }
            // std::cout << i << " ";
        }
    } else {
        context.mpi_sort(nullptr, nullptr);
    }
}
