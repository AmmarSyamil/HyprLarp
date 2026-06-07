#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <fstream>

#include "DataType.hpp"





int CommandTest() {
    const char* command = "mpv --vo=kitty --vo-kitty-use-shm=yes --osc=no --osd-level=0 --profile=sw-fast --really-quiet --no-terminal video.mp4";
    int result = std::system(command);
    return result;
}

// Function to check how fast mpv on kity were actually is
// result :  Total Elapsed Time: 1107 ms with 1000 ms video, so its 107 average
int SpeedTest() {

    int final_result{};

    final_result = 0;
    std::ofstream outFile("output.txt");

    for (size_t i = 0; i < 10; i++)
    {    
        auto start = std::chrono::high_resolution_clock::now();
    
        CommandTest();
    
        auto finish = std::chrono::high_resolution_clock::now();
    
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
        
        final_result = final_result + elapsed;
        outFile << i << " " << elapsed << " ms\n";

    }
    

    
    if (outFile.is_open()) {
        outFile << " Total Elapsed Time: " << final_result / 10 << " ms\n";
        outFile.close();
        
        // This is just a backup console print, but the file is your source of truth
        std::cout << "[Done] Results successfully written to output.txt" << std::endl;
    } else {
        std::cerr << "Error: Could not open output.txt for writing." << std::endl;
    }

    return 0;
};

int renderVideo() {
    // build command string (use std::string to concatenate)
    std::string command = std::string("mpv --vo=kitty --vo-kitty-use-shm=yes --osc=no --osd-level=0 --profile=sw-fast --really-quiet --no-terminal")
        + " --vf=crop=width:height:x_offset:y_offset video.mp4";
    int result = std::system(command.c_str());
    return result;
}



// int main() {
//     SpeedTest();
//     return 0;
// }