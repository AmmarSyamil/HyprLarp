// File contain the implementation in the client/consumer which is the terminal
// consumer.hpp
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
#include "videoDecoder.hpp"

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
        if (width == 0 || height == 0) {
            std::cerr << "dispalyImage : unvalid width and height" << std::endl;
            return -1;
        }

        escSequence(width, height, image_size, SHMfileName);
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
        getImageData();
        shmPtr = openSHM();
        // displayImage();
    }

    ~consumer() {
        if (shmPtr) {
            size_t total_mapped_size = sizeof(VideoHeader) + image_size;
            exitSHM(shmPtr, total_mapped_size);
        }
    }
};
