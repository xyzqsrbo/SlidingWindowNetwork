#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>

int main()
{

    std::chrono::time_point<std::chrono::steady_clock> weirdStart;

    auto epoch = weirdStart;
    auto win_s = std::chrono::time_point_cast<std::chrono::milliseconds>(epoch);
    auto value = win_s.time_since_epoch();
    long windowVal = value.count();
    std::cout << "Window Value: " << windowVal << std::endl;

    auto start = std::chrono::steady_clock::now();

    weirdStart = std::chrono::steady_clock::now();

    epoch = weirdStart;
    win_s = std::chrono::time_point_cast<std::chrono::milliseconds>(epoch);
    value = win_s.time_since_epoch();
    windowVal = value.count();
    std::cout << "Window Value: " << windowVal << std::endl;

    sleep(8);

    auto end = std::chrono::steady_clock::now();

    double elapsed_time = double(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    double weirdElapsed = double(std::chrono::duration_cast<std::chrono::milliseconds>(end - weirdStart).count());

    std::cout << "Elapsed Time (ms): " << elapsed_time << std::endl;
    std::cout << "Weird Elapsed Time (ms): " << weirdElapsed << std::endl;

    return 0;
}