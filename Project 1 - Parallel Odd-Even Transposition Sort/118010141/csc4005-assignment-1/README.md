# Odd-Even Transposition Sort

Please implement an Odd-Even Transposition Sort algorithm using MPI.

-    For each process with odd rank P, send its number to the process with rank P-1.

-    For each process with rank P-1, compare its number with the number sent by the process with rank P and send the larger one back to the process with rank P.

-    For each process with even rank Q, send its number to the process with rank Q-1.

-    For each process with rank Q-1, compare its number with the number sent by the process with rank Q and send the larger one back to the process with rank Q.

-    Repeat 1-4 until the numbers are sorted.


# About the Template

- Modify the code in `src/odd-even-sort.cpp`
- The project can be compiled as the following:
  ```bash
  cd /path/to/project
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Debug # please modify this to `Release` if you want to benchmark your program
  cmake --build . -j4
  ```
- The project will generate two executables:
  - main: the main project accepts two arguments: input and output. It reads all numbers from input and print the sorted results to the output.
  - gtest_sort: the test program contains two simple test cases for you to check the correctness of the program.

 
