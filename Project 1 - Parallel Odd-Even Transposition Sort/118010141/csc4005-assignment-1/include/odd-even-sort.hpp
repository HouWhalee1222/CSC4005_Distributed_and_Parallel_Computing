#pragma once

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <vector>

namespace sort {
    using Element = int64_t;
    /** Information
     *  The information for a single mpi run
     */
    struct Information {
        std::chrono::high_resolution_clock::time_point start{};
        std::chrono::high_resolution_clock::time_point end{};
        size_t length{};  // length of the array to be sorted
        int num_of_proc{};  // number of processes
        int argc{};
        std::vector<char *> argv{};  // Arguments
    };

    struct Context {
        int argc;
        char **argv;

        Context(int &argc, char **&argv);

        ~Context();


        /**!
         * Swap two elements
         * @param first first position
         * @param second second position
         */
        void swapE(Element* first, Element* second) const;

        /**!
         * Odd sort process, sort from the first element
         * @param localArray array to be odd sorted
         * @param localCount total numbers in the array
         */
        void oddSort(Element* localArray, int localCount) const;

        /**!
         * Even sort process, sort from the second element
         * @param localArray array to be odd sorted
         * @param localCount total numbers in the array
         */
        void evenSort(Element* localArray, int localCount) const;

        /**!
         * Sort the elements in range [begin, end) in the ascending order.
         * For sub-processes, null pointers will be passed. That is, the root process
         * should be in charge of sending the data to other processes.
         * @param begin starting position
         * @param end ending position
         * @return the information for the sorting
         */
        std::unique_ptr<Information> mpi_sort(Element *begin, Element *end) const;

        /*!
         * Print out the information.
         * @param info information struct
         * @param output output stream
         * @return the output stream
         */
        static std::ostream &print_information(const Information &info, std::ostream &output);
    };
}