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
#include <filesystem>

class consumer
{
private:
    uint8_t* ProducerSHMPtr = nullptr; // ptr to main producer shm file
    uint8_t* shmPtr = nullptr; // pointer to the shm of the producer 
    std::string SHMfileName;
    int width = 0;
    int height = 0;
    int image_size = 0;
    int currentFrame = 0;
    int pid = 0;
    int frame = 0;
    int videoHeaderSize = 0;

public:

    // Create unique filename for SHM to differenciate SHM file from different frame and different terminal
    int setupSHMfileName() {
        // initialize pid so the generated name matches the current process
        pid = getpid();
        std::string fileNameSHM = "/HyprLarp:" + std::to_string(pid) + ":" + std::to_string(frame);

        SHMfileName = fileNameSHM;
        std::cout << "setupSHMfileName : " << SHMfileName << std::endl;


        return 1;
    }

    // Function to setup the SHM recuire each frame
    int setupSHM() {
        std::cout << "pre setupSHM : " << SHMfileName  << std::endl;
        // Create display SHM WITHOUT header (for Kitty graphics protocol)
        shmPtr = createSHM(image_size, width, height, SHMfileName, false);
        std::cout << "post setupSHM : " << SHMfileName << std::endl;

        std::string real_path = "/dev/shm/" + SHMfileName.substr(1);
    
        if (std::filesystem::exists(real_path)) {
            std::cout << "The file exists at " << real_path << " right now!" << std::endl;
        } else {
            std::cout << "The file is genuinely not at " << real_path << std::endl;
        }

        return 1;
    }

    int populateSHM() {
        if (!ProducerSHMPtr) {
            std::cerr << "populateSHM: Producer SHM pointer is null" << std::endl;
            return -1;
        }

        // ProducerSHMPtr points to mapping start (VideoHeader). Advance to pixel data.
        putSHM(shmPtr, ProducerSHMPtr + sizeof(VideoHeader), image_size);

        return 1;
    }

    int displayImage() {
        if (width == 0 || height == 0) {
            std::cerr << "dispalyImage : unvalid width and height" << std::endl;
            return -1;
        }

        // Check SHM
        std::string real_path = "/dev/shm/" + SHMfileName.substr(1);
        if (std::filesystem::exists(real_path)) {
            std::cout << "The file exists at for pre display " << real_path << " right now!" << std::endl;
        } else {
            std::cout << "The file is genuinely not at pre display " << real_path << std::endl;
        }

        // Test SHM
        int test = testSHM(image_size, width, height, SHMfileName);

        if (test == -1) {
            std::cout << "consumer.hpp displayImage : cant test SHM" << std::endl;
            return -1;
        }


        std::cout << "pre escsequence shmfilename " << SHMfileName << std::endl;
        escSequence(width, height, image_size, SHMfileName, videoHeaderSize);
        std::cout << "displayImage : escSequences runned" << std::endl;

        return 1;
    }

    int getImageData() {
        std::vector<int> data = getImageSHM();
        
        // Validate returned data
        if (data.size() != 4) {
            std::cerr << "getImageData: Invalid return vector size: " << data.size() << std::endl;
            return -1;
        }
        
        width = data[0];
        height = data[1];
        image_size = data[2];
        videoHeaderSize = data[3];
        
        // Check for valid dimensions
        if (width <= 0 || height <= 0 || image_size <= 0) {
            std::cerr << "getImageData: Invalid video dimensions - width=" << width 
                      << ", height=" << height << ", image_size=" << image_size << std::endl;
            std::cerr << "getImageData: Producer SHM (/vp_static) not ready. Start producer first." << std::endl;
            width = height = image_size = videoHeaderSize = 0;
            return -1;
        }

        return 1;
    }

    consumer() {
        int result = getImageData(); // Populate basic video data
        
        if (result != 1) {
            std::cerr << "consumer: Failed to get image data from producer SHM" << std::endl;
            return;
        }

        // Testing
        std::cout << "Testing ground first one " << height << " " <<width << " " << image_size << std::endl;

        ProducerSHMPtr = openSHM(); // Setup shm ptr 
        if (!ProducerSHMPtr) {
            std::cerr << "consumer: Failed to open producer SHM" << std::endl;
            return;
        }
        // displayImage();
    }

    ~consumer() {
        if (shmPtr) {
            size_t total_mapped_size = sizeof(VideoHeader) + image_size;
            exitSHM(shmPtr, total_mapped_size); 
        }
    }
};