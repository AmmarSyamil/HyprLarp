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
    int pid = 0;
    int videoHeaderSize = 0;
    uint64_t last_sequence = static_cast<uint64_t>(-1);

public:
    int frame = 0;

    // Create unique filename for SHM to differentiate SHM file from different frame and different terminal
    int setupSHMfileName(int currectFrame) {
        pid = getpid();
        frame = currectFrame;
        std::string fileNameSHM = "HyprLarp:" + std::to_string(pid) + ":" + std::to_string(frame);

        SHMfileName = fileNameSHM;
//         std::cout << "setupSHMfileName : " << SHMfileName << std::endl;

        return 1;
    }

    // Function to create the SHM recuire each frame
    int setupSHM() {
        if (shmPtr) {
            exitSHM(shmPtr, static_cast<size_t>(image_size));
            shmPtr = nullptr;
        }

//         std::cout << "pre setupSHM : " << SHMfileName  << std::endl;
        shmPtr = createSHM(width, height, SHMfileName, false);
//         std::cout << "post setupSHM : " << SHMfileName << std::endl;

        return 1;
    }

    // Function to read the SHM from the producer and put it into the consumer's SHM
    int populateSHM() {
        if (!ProducerSHMPtr) {
            std::cerr << "populateSHM: Producer SHM pointer is null" << std::endl;
            return -1;
        }

        controlHeader* header = reinterpret_cast<controlHeader*>(ProducerSHMPtr);
        while (true) {
            uint64_t seq = header->global_sequences.load(std::memory_order_acquire);
            if (seq == last_sequence) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            uint32_t slot_index = header->write_slot_index.load(std::memory_order_acquire);
            uint64_t seq_check = header->global_sequences.load(std::memory_order_acquire);
            if (seq != seq_check) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (static_cast<int>(slot_index) < 0 || slot_index >= header->num_sloth) {
                std::cerr << "populateSHM: invalid producer slot index " << slot_index << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            std::vector<uint8_t> frame_data(image_size);
            if (readFrameFromSlot(ProducerSHMPtr, slot_index, frame_data.data()) == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (putSHM(shmPtr, frame_data.data(), image_size) == -1) {
                std::cerr << "populateSHM: cant put data into the SHM" << std::endl;
                return -1;
            }

            last_sequence = seq;
            return 1;
        }
    }

    // Function to display the image of the SHM
    int displayImage() {
        if (width == 0 || height == 0) {
            std::cerr << "dispalyImage : unvalid width and height" << std::endl;
            return -1;
        }

        // Check SHM
        std::string real_path = std::string("/dev/shm/") + SHMfileName;
        if (std::filesystem::exists(real_path)) {
//             std::cout << "The file exists at for pre display " << real_path << " right now!" << std::endl;
        } else {
//             std::cout << "The file is genuinely not at pre display " << real_path << std::endl;
        }

        // Test SHM
        int test = testSHM(image_size, width, height, SHMfileName);

        if (test == -1) {
//             std::cout << "consumer.hpp displayImage : cant test SHM" << std::endl;
            return -1;
        }


//         std::cout << "pre escsequence shmfilename " << SHMfileName << std::endl;
        escSequence(width, height, image_size, SHMfileName, videoHeaderSize);
//         std::cout << "displayImage : escSequences runned" << std::endl;

        return 1;
    }

    // Get metadata of the file
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

    // Class constructor
    consumer() {
        int result = getImageData(); // Populate basic video data
        
        if (result != 1) {
            std::cerr << "consumer: Failed to get image data from producer SHM" << std::endl;
            return;
        }

        // Testing
//         std::cout << "Testing ground first one " << height << " " <<width << " " << image_size << std::endl;

        ProducerSHMPtr = openSHM(); // Setup shm ptr 
        if (!ProducerSHMPtr) {
            std::cerr << "consumer: Failed to open producer SHM" << std::endl;
            return;
        }
        // displayImage();
    }

    ~consumer() {
        if (ProducerSHMPtr) {
            size_t total_mapped_size = static_cast<size_t>(videoHeaderSize) + static_cast<size_t>(image_size) * 8;
            exitSHM(ProducerSHMPtr, total_mapped_size);
            ProducerSHMPtr = nullptr;
        }

        if (shmPtr) {
            exitSHM(shmPtr, static_cast<size_t>(image_size));
            shm_unlink(SHMfileName.c_str());
            shmPtr = nullptr;
        }
    }
};