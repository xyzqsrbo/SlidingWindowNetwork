#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>

int main(){
    
    auto start = std::chrono::steady_clock::now();

    sleep(8);

    auto end = std::chrono::steady_clock::now();

    double elapsed_time = double(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    std::cout << "Elapsed Time (ms): " << elapsed_time << std::endl;

    return 0;
}