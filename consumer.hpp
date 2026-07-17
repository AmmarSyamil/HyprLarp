// File contain the implementation in the client/consumer which is the terminal
// consumer.hpp
#pragma once

#include <iostream>
#include "DataType.hpp"
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
#include "base64converter.hpp"
#include "layoutSHM.hpp"
#include "checkTerminal.hpp"
#include "consumerLayout.hpp"
#include <sys/ioctl.h>
#include <fstream>
#include "terminal.hpp"
#include <chrono>
#include "renderer.hpp"
#include <thread>
#include "consumerRegistry.hpp"


// universal logging
static void tlog(const char* tag, const std::string& msg) {
    static std::ofstream dbg("/tmp/hyprlarp_unified.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " [" << tag << ":" << FindTerminalPID() << "] " << msg << std::endl;
    dbg.flush();
}

// loggin 
static void hb(const char* where) {
    static std::ofstream dbg("/tmp/hyprlarp_heartbeat.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " " << where << std::endl;
    dbg.flush();
}

class consumer
{
private:
    InternalTerminalGeometry geo;
    std::string BaseSHMName;
    std::string pendingKittyUnlink;
    LayoutRender layoutRender;
    ViewportState viewPort;

    bool checkHyprlandEvent();


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
    bool layoutReady = false;

public:
    int frame = 0;

    // test debug
    pid_t terminalPid = 0;
    bool refreshLayout() {
        return computeConsumerLayout(terminalPid, width, height, layoutRender, viewPort, geo);
    }

    // Enable debug mode (shows timing stats instead of video)
    void enableDebugMode() { debug_mode = true; }
    void disableDebugMode() { debug_mode = false; }

    // Create unique filename for SHM to differentiate SHM file from different frame and different terminal
    int setupSHMfileName(int currectFrame) {
        pid = FindTerminalPID();
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

        shmPtr = createSHM(width, height, SHMfileName, false);

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

    void logRenderFrameSkip(const char* reason, uint64_t global_seq, uint64_t target_seq) {
        std::ofstream dbg("/tmp/hyprlarp_render_debug.log", std::ios::app);
        dbg << "skip: " << reason << " global_seq=" << global_seq
            << " target_seq=" << target_seq
            << " last_sequence=" << last_sequence
            << std::endl;
    }


    // Render frame
    int renderFrame();
    
    // Debug mode: display frame timing stats while showing video
    int displayDebugStats() {
        auto now = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration<double>(now - frame_start_time).count();
        auto delta_from_last = last_frame_time.time_since_epoch().count() == 0 ? 0.0 : 
                               std::chrono::duration<double>(now - last_frame_time).count();
        
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
            std::cerr << "getImageData: Producer SHM (/HyprLarp-Producer) not ready. Start producer first." << std::endl;
            width = height = image_size = videoHeaderSize = 0;
            return -1;
        }

        return 1;
    }

    // Class constructor
    // act just for nulling value, use init function for gathering initial data
    consumer() {
        ProducerSHMPtr = nullptr;
        shmPtr = nullptr;
        width = height = image_size = 0;
        last_sequence = static_cast<uint64_t>(-1);
    }

    ~consumer() {
        if (ProducerSHMPtr) {
            size_t total_mapped_size = static_cast<size_t>(videoHeaderSize) + static_cast<size_t>(image_size) * RING_BUFFER_SLOTS;
            exitSHM(ProducerSHMPtr, total_mapped_size);
            ProducerSHMPtr = nullptr;
        }

        if (!pendingKittyUnlink.empty()) {
            shm_unlink(("/" + pendingKittyUnlink).c_str());
            pendingKittyUnlink.clear();
        }

        if (shmPtr) {
            exitSHM(shmPtr, static_cast<size_t>(image_size));
            shm_unlink(SHMfileName.c_str());
            shmPtr = nullptr;
        }

        unRegisterConsumer(FindTerminalPID());
    }

    bool fetchLayout() {
        // if (width <= 0 || height <= 0) {
        //     return false;
        // }

        // static auto lastUpdate = std::chrono::steady_clock::time_point::min();
        // auto now = std::chrono::steady_clock::now();
        // if (layoutReady && lastUpdate != std::chrono::steady_clock::time_point::min()
        //     && now - lastUpdate < std::chrono::milliseconds(200)) {
        //     return true;
        // }

        // pid_t myPid = FindTerminalPID();
        // if (myPid <= 0) {
        //     std::cerr << "fetchLayout: could not find terminal PID\n";
        //     return false;
        // }

        // if (!computeConsumerLayout(myPid, width, height, layoutRender, viewPort)) {
        //     return false;
        // }

        // layoutReady = true;
        // lastUpdate = now;

        // std::ofstream logFile("/tmp/layout.log", std::ios_base::app);
        // if (logFile.is_open()) {
        //     logFile << "fetchLayout(local): x=" << layoutRender.x << " y=" << layoutRender.y
        //             << " w=" << layoutRender.w << " h=" << layoutRender.h
        //             << " cols=" << layoutRender.disp_cols << " rows=" << layoutRender.disp_rows
        //             << " cursor=" << layoutRender.cursor_col << "," << layoutRender.cursor_row
        //             << " sub=" << layoutRender.sub_offset_x << "," << layoutRender.sub_offset_y
        //             << " pid=" << myPid << std::endl;
        //     logFile.close();
        // }

        // return true;

        // debug test
        return computeConsumerLayout(terminalPid, width, height, layoutRender, viewPort, geo);
    }

    // Function to setup the class
    bool init();
};

int mainConsumer();