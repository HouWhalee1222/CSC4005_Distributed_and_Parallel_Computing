#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdlib>

using namespace std;

int main(int argc, char **argv) {
    srand((int)time(0));
    int n = atoi(argv[1]);
    ofstream outFile(argv[2]);
    int count = 1;
    for (int i = 0; i < n; i++) {
        outFile << rand() % RAND_MAX << " ";
        if (count++ % 20 == 0) {
            outFile << endl;
        } 
    }
    return 0;
}