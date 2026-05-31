// File contain the implementation in the client/consumer which is the terminal
#pragma once

#include <iostream>
#include "shm.hpp"
#include <vector>

#include "renderer.hpp"
#include "shm.hpp"
extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

class consumer
{
private:
    uint8_t* shmPtr = nullptr;
    std::string SHMfileName = "/vp_static";
    int width = 0;
    int height = 0;
    int image_size = 0;
    int currentFrame = 0;

public:
    int displayImage() {
        escSequence(width, height, SHMfileName);
        std::cout << "displayImage : escSequences runned" << std::endl;

        return 1;
    }

    int getImageData() {
        std::vector<int> data = getImageSHM();
        width = data[0];
        height = data[1];
        image_size = data[2];

        return 1;
    }


    consumer() {
        shmPtr = openSHM();
        getImageData();
        displayImage();
    }
};
