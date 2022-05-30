#include <odd-even-sort.hpp>
#include <mpi.h>
#include <iostream>
#include <vector>

#define MASTER 0

namespace sort {
    using namespace std::chrono;


    Context::Context(int &argc, char **&argv) : argc(argc), argv(argv) {
        MPI_Init(&argc, &argv);
    }

    Context::~Context() {
        MPI_Finalize();
    }

    void Context::swapE(Element* first, Element* second) const {
        Element temp = *second;
        *second = *first;
        *first = temp;
    }

    void Context::oddSort(Element* localArray, int localCount) const {
        for (int j = 0; j < localCount - 1; j += 2) {
            if (*(localArray + j) > *(localArray + j + 1)) {
                swapE(localArray + j, localArray + j + 1);
            }
        }
    }

    void Context::evenSort(Element* localArray, int localCount) const {
        for (int j = 1; j < localCount - 1; j += 2) {
            if (*(localArray + j) > *(localArray + j + 1)) {
                swapE(localArray + j, localArray + j + 1);
            }
        }
    }

    std::unique_ptr<Information> Context::mpi_sort(Element *begin, Element *end) const {
        int res;
        int rank;
        int size;
        int totalCount;  // total number of elements
        int localCount;  // the number of local array elements
        int remain;

        Element buffer;

        Element* localArray;

        std::unique_ptr<Information> information{};

        res = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if (MPI_SUCCESS != res) {
            throw std::runtime_error("failed to get MPI world rank");
        }
        res = MPI_Comm_size(MPI_COMM_WORLD, &size);
        if (MPI_SUCCESS != res) {
            throw std::runtime_error("failed to get MPI world size");
        }

        if (rank == MASTER) {
            information = std::make_unique<Information>();
            information->length = end - begin;
            information->num_of_proc = size;
            information->argc = argc;
            for (auto i = 0; i < argc; ++i) {
                information->argv.push_back(argv[i]);
            }
            information->start = high_resolution_clock::now();

            totalCount = information->length;
        }

        // Broadcast total number count
        MPI_Bcast(&totalCount, 1, MPI_INT, 0, MPI_COMM_WORLD);

        localCount = totalCount / size;
        remain = totalCount - localCount * size;

        if (rank == MASTER) {
            localCount += remain;
        }

        // Malloc the space to store local elements
        localArray = (Element*)malloc(localCount*sizeof(Element));

        // Distribute all the numbers into the slave processes evenly
        MPI_Scatter(begin + remain, totalCount / size, MPI_LONG, localArray, totalCount / size, MPI_LONG, MASTER, MPI_COMM_WORLD);

        // Move the number in the root process to normal order
        if (rank == MASTER && remain != 0) {
            for (int i = localCount - 1; i >= 0; i--) {
                *(localArray + i) = *(localArray + i - remain); 
            }
            for (int i = 0; i < remain; i++) {
                *(localArray + i) = *(begin + i);
            }
        }
        
        // std::cout << "I am process " << rank << std::endl;
        // for (int i = 0; i < localCount; i++) {
        //     std::cout << localArray[i] << std::endl;
        // }

        // Start odd-even sort
        for (int i = 0; i < totalCount; i++) {  // Fixed times sorting
            // Divide the case by rank number
            if (rank == MASTER) {
                if (i % 2 == 0) {  // odd phase 
                    oddSort(localArray, localCount);
                } else {  // even phase
                    evenSort(localArray, localCount);
                }
                
                if (localCount % 2 != i % 2 && size > 1) {  // Will send a number
                    MPI_Send(localArray + localCount - 1, 1, MPI_LONG, 1, MASTER, MPI_COMM_WORLD);
                    MPI_Recv(&buffer, 1, MPI_LONG, 1, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    *(localArray + localCount - 1) = buffer;
                }
            } else if (rank < size - 1) {
                int previous = localCount * rank + remain;
                if (previous % 2 == i % 2) {  // The number of previous elements is even
                    oddSort(localArray, localCount);
                    if (localCount % 2 == 1) {  // need to send a number to next process
                        MPI_Send(localArray + localCount - 1, 1, MPI_LONG, rank + 1, MASTER, MPI_COMM_WORLD);
                        MPI_Recv(&buffer, 1, MPI_LONG, rank + 1, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        *(localArray + localCount - 1) = buffer;
                    }
                } else {  // The number of previous elements is odd, receive one from last process
                    evenSort(localArray, localCount);
                    MPI_Recv(&buffer, 1, MPI_LONG, rank - 1, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (buffer > localArray[0]) {
                        swapE(&buffer, localArray);
                    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
                    MPI_Send(&buffer, 1, MPI_LONG, rank - 1, MASTER, MPI_COMM_WORLD);
                    if (localCount % 2 == 0 && localCount > 1) { // even element, need to send one to the next process 
                        MPI_Send(localArray + localCount - 1, 1, MPI_LONG, rank + 1, MASTER, MPI_COMM_WORLD);
                        MPI_Recv(&buffer, 1, MPI_LONG, rank + 1, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        *(localArray + localCount - 1) = buffer;
                    }
                }
            } else {  // The last process
                int previous = localCount * rank + remain;
                if (previous % 2 == i % 2) {  // No element from previous processes
                    oddSort(localArray, localCount);
                } else {
                    evenSort(localArray, localCount);
                    MPI_Recv(&buffer, 1, MPI_LONG, rank - 1, MASTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (buffer > localArray[0]) {
                        swapE(&buffer, localArray);
                    }
                    MPI_Send(&buffer, 1, MPI_LONG, rank - 1, MASTER, MPI_COMM_WORLD);
                }
            }


            // std::cout << "I am process " << rank << " after sorting " << i << ": ";
            // for (int i = 0; i < localCount; i++) {
            //     std::cout << localArray[i] << " ";
            // }
            // std::cout << std::endl;
        }

        if (rank == MASTER) {
            for (int i = 0; i < remain; i++) {
                *(begin + i) = localArray[i];
            }
            localArray += remain;
            localCount -= remain;
        }
        
        MPI_Gather(localArray, localCount, MPI_LONG, begin, localCount, MPI_LONG, MASTER, MPI_COMM_WORLD);
        
        if (rank == MASTER) {
            localArray -= remain;
        }

        free(localArray);
        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == MASTER) {
            information->end = high_resolution_clock::now();
        }

        return information;
    }

    std::ostream &Context::print_information(const Information &info, std::ostream &output) {
        auto duration = info.end - info.start;
        auto duration_count = duration_cast<nanoseconds>(duration).count();
        output << "Name: Li Jingyu" << std::endl; 
        output << "Student ID: 118010141" << std::endl;
        output << "Assignment 1, odd-even sort, MPI implementation" << std::endl;
        output << "input size: " << info.length << std::endl;
        output << "proc number: " << info.num_of_proc << std::endl;
        output << "duration (ns): " << duration_count << std::endl;
        return output;
    }
}
