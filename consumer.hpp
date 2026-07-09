// File contain the implementation in the client/consumer which is the terminal
// consumer.hpp
#pragma once

#include <iostream>
#include "shm.hpp"
#include <stdexcept>
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
    
    // Debug tracking
    bool debug_mode = false;
    uint32_t last_slot = 0;
    int poll_retries = 0;
    std::chrono::high_resolution_clock::time_point frame_start_time;
    std::chrono::high_resolution_clock::time_point last_frame_time;
    std::vector<uint8_t> frame_data_cache;  // Pre-allocated frame buffer


public:
    int frame = 0;
    
    // Enable debug mode (shows timing stats instead of video)
    void enableDebugMode() { debug_mode = true; }
    void disableDebugMode() { debug_mode = false; }

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

        frame_start_time = std::chrono::high_resolution_clock::now();
        poll_retries = 0;
        controlHeader* header = reinterpret_cast<controlHeader*>(ProducerSHMPtr);
        while (true) {
            uint64_t seq = header->global_sequences.load(std::memory_order_acquire);
            if (seq == last_sequence) {
                poll_retries++;
                std::this_thread::sleep_for(std::chrono::microseconds(250));
                continue;
            }

            uint32_t slot_index = header->write_slot_index.load(std::memory_order_acquire);
            uint64_t seq_check = header->global_sequences.load(std::memory_order_acquire);
            if (seq != seq_check) {
                poll_retries++;
                std::this_thread::sleep_for(std::chrono::microseconds(250));
                continue;
            }

            if (static_cast<int>(slot_index) < 0 || slot_index >= header->num_sloth) {
                std::cerr << "populateSHM: invalid producer slot index " << slot_index << std::endl;
                std::this_thread::sleep_for(std::chrono::microseconds(250));
                continue;
            }

            // Pre-allocate frame buffer on first use
            if (frame_data_cache.empty()) {
                frame_data_cache.resize(image_size);
            }

            if (readFrameFromSlot(ProducerSHMPtr, slot_index, frame_data_cache.data()) == -1) {
                poll_retries++;
                std::this_thread::sleep_for(std::chrono::microseconds(250));
                continue;
            }

            if (putSHM(shmPtr, frame_data_cache.data(), image_size) == -1) {
                std::cerr << "populateSHM: cant put data into the SHM" << std::endl;
                return -1;
            }

            last_slot = slot_index;
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
            // std::cerr << "consumer"
            throw std::runtime_error("consumer displayImage : Failed to find corespond SHMfile");
        }

        escSequence(width, height, image_size, SHMfileName, videoHeaderSize);

        return 1;
    }

    // Debug mode: display frame timing stats while showing video
    int displayDebugStats() {
        auto now = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration<double>(now - frame_start_time).count();
        auto delta_from_last = last_frame_time.time_since_epoch().count() == 0 ? 0.0 : 
                               std::chrono::duration<double>(now - last_frame_time).count();
        
        // Print debug stats to stderr (doesn't interfere with video on stdout)
        std::cerr << "[CONSUMER] Seq=" << last_sequence 
                  << " Slot=" << last_slot 
                  << " Polls=" << poll_retries 
                  << " ReadTime=" << frame_duration*1000 << "ms"
                  << " Delta=" << delta_from_last*1000 << "ms" << std::endl;
        
        last_frame_time = now;
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

int mainConsumer();