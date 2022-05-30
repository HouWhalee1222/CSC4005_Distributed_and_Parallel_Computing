#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstddef>
#include <memory>
#include <cstdint>

using namespace std;
using Element = int64_t;

chrono::high_resolution_clock::time_point startt;
chrono::high_resolution_clock::time_point endt;

void swapE(Element *first, Element *second) {
    Element temp = *second;
    *second = *first;
    *first = temp;
}

void odd_even_sort(Element *begin, Element *end) {
    int size = end - begin;
    startt = chrono::high_resolution_clock::now();

    for (int i = 0; i < size / 2 + 1; i++) {  // Fixed times sorting
        
        // Odd phase
        for (int j = 0; j < size - 1; j += 2) {
            if (*(begin + j) > *(begin + j + 1)) {
                swapE(begin + j, begin + j + 1);
            }
        }

        // Even phase
        for (int j = 1; j < size - 1; j += 2) {
            if (*(begin + j) > *(begin + j + 1)) {
                swapE(begin + j, begin + j + 1);
            }
        }
    }

    endt = chrono::high_resolution_clock::now();
}

void print_information(int size) {
    auto duration = endt - startt;
    auto duration_count = chrono::duration_cast<chrono::nanoseconds>(duration).count();
    auto mem_size = static_cast<double>(size) * sizeof(Element) / 1024.0 / 1024.0 / 1024.0;
    cout << "Name: Li Jingyu" << std::endl; 
    cout << "Student ID: 118010141" << std::endl;
    cout << "Assignment 1, odd-even sort, MPI implementation" << std::endl;
    cout << "input size: " << size << std::endl;
    cout << "duration (ns): " << duration_count << std::endl;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "wrong arguments" << endl;
        cerr << "usage: " << argv[0] << " <input-file> <output-file>" << endl;
        return 0;
    }
    
    Element element;
    vector<Element> data;
    ifstream input(argv[1]);
    ofstream output(argv[2]);
    while (input >> element) {
        data.push_back(element);
    }
    odd_even_sort(data.data(), data.data() + data.size());
    print_information(int(data.size()));
    for (auto i : data) {
        output << i << " ";
    }


}
