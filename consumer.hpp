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
    uint8_t* ProducerSHMPtr = nullptr; // ptr to main producer shm file
    uint8_t* shmPtr = nullptr; // pointer to the shm of the producer 
    const char * SHMfileName;
    int width = 0;
    int height = 0;
    int image_size = 0;
    int currentFrame = 0;
    int pid = 0;
    int frame = 0;

public:

    // Create unique filename for SHM to differenciate SHM file from different frame and different terminal
    int setupSHMfileName() {
        std::string fileName = "/HyprLarp:" + std::to_string(pid) + ":" + std::to_string(frame);

        SHMfileName = fileName.c_str();
        std::cout << "setupSHMfileName : " << SHMfileName << std::endl;


        return 1;
    }

    // Function to setup the SHM recuire each frame
    int setupSHM() {
        std::cout << "pre setupSHM : " << SHMfileName << std::endl;
        shmPtr = createSHM(image_size, width, height, SHMfileName); // result in consumer ptr shm
        std::cout << "post setupSHM : " << SHMfileName << std::endl;

        return 1;
    }

    int populateSHM() {
        putSHM(shmPtr, ProducerSHMPtr, image_size);

        return 1;
    }

    int displayImage() {
        if (width == 0 || height == 0) {
            std::cerr << "dispalyImage : unvalid width and height" << std::endl;
            return -1;
        }

        std::cout << "pre escsequence shmfilename " << SHMfileName << std::endl;
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
        getImageData(); // Populate basic video data

        // Testing
        std::cout << "Testing ground first one " << height << " " <<width << " " << image_size << std::endl;

        ProducerSHMPtr = openSHM(); // Setup shm ptr 
        // displayImage();
    }

    ~consumer() {
        if (shmPtr) {
            size_t total_mapped_size = sizeof(VideoHeader) + image_size;
            exitSHM(shmPtr, total_mapped_size); 
        }
    }
};