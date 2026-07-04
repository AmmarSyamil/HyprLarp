// Renderer.cpp
#include <iostream>
#include <unistd.h>
#include <string>

#include "base64converter.hpp"
#include <filesystem>


// Function to display to write the escape sequences
int escSequence(int width, int height, int image_size, std::string& imageSHM, int videoHeaderSize) {

    // test shmfilename
    std::string SHMfileName = imageSHM;
    std::string real_path = "/dev/shm/" + SHMfileName;
    if (std::filesystem::exists(real_path)) {
//             std::cout << "The file exists at for pre display " << real_path << " right now!" << std::endl;
    } else {
        std::cerr << "The file is genuinely not at pre display " << real_path << std::endl;
    }

    std::string shm_basename = imageSHM;
    // if (!shm_basename.empty() && shm_basename[0] == '/') {
        // shm_basename = shm_basename.substr(1);
    // }

    // std::cerr << "Base64  base encoded: '" << shm_basename<< "'\n";

    std::string b64_shm_name = base64Converter(shm_basename);

//     std::cout << "escsequence start" << std::endl;
//     std::cout << "escsequence imageSHM name " << imageSHM << std::endl;
//     std::cout << "escsequence Image SHM name encoded " << b64_shm_name << std::endl;

    int pixel_data_sizes = width * height * 4;

    // Create the escape sequence string
    std::string escape = "\x1b_Ga=T,f=32,s=" + std::to_string(width) + 
                     ",v=" + std::to_string(height) + 
                     ",t=s,O=0,S=" + std::to_string(pixel_data_sizes) +
                     ",i=1"
                     ",q=2;" + b64_shm_name + "\x1b\\";

    // Cursor
    write(STDOUT_FILENO, "\x1b[H", 3); // Move cursor to top-left corner
    
    // Using writeFirst pixel byte:
    write(STDOUT_FILENO, escape.c_str(), escape.size());
    fflush(stdout);

    // Using  cout and flush
    // std::cout << escape << std::flush;

    // if (!isatty(STDOUT_FILENO)) {
    //     std::cerr << "Error: stdout is not a terminal!" << std::endl;
    // }

    // if (write(STDOUT_FILENO, escape.c_str(), escape.size()) == -1) {
    //     perror("Failed to write escape sequence");
    //     return 0;
    // }

    // std::cerr << "Escape sequence hex: ";
    // for (unsigned char c : escape) {
    //     fprintf(stderr, "%02x ", c);
    // }
    fprintf(stderr, "\n");
    
//     std::cout << "displayed finish" << std::endl;

    return 1;
}



