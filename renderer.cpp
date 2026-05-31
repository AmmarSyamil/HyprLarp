#include <iostream>
#include <unistd.h>
#include <string>

// Function to create copy of the buffer
// idk actually

// Function to display to write the escape sequences
int escSequence(int width, int height, std::string imageSHM) {
    std::string b64_shm_name = "L3ZwX3N0YXRpYw==";
    std::string escape = "\x1b_Gf=32,s=" + std::to_string(width) + ",v=" + std::to_string(height) + ",t=s,O=12,q=2;" + b64_shm_name+ "\x1b\\";
    
    write(STDOUT_FILENO, escape.c_str(), escape.size());

    return 1;
}

