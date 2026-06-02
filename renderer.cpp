// Renderer.cpp
#include <iostream>
#include <unistd.h>
#include <string>

#include "base64converter.hpp"

// Function to create copy of the SHM

// Function to display to write the escape sequences
int escSequence(int width, int height, int image_size, const char * imageSHM) {
    // std::string b64_shm_name = base64Converter(imageSHM);
    std::string imageName = imageSHM;

    std::cout << "escsequence start" << std::endl;
    std::cout << imageSHM << std::endl;
    std::cout << imageName << std::endl;

    // int pixel_data_bytes = width * height * 4;
    std::string escape = "\x1b_Gf=32,s=" + std::to_string(width) + 
                         ",v=" + std::to_string(height) + 
                         ",t=s,O=12,q=2;" + imageSHM + "\x1b\\";
    
    // Using write
    // write(STDOUT_FILENO, escape.c_str(), escape.size());

    // Using  cout and flush
    // std::cout << escape << std::flush;

    if (write(STDOUT_FILENO, escape.c_str(), escape.size()) == -1) {
        perror("Failed to write escape sequence");
        return 0;
    }
    std::cout << "displayed finish" << std::endl;

    return 1;
}

