#include <iostream>
#include <string>
#include <chrono>
#include "videoDecoder.hpp"
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
}


// Function to idk
int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame) {
    auto start_time = std::chrono::high_resolution_clock::now();

    while (decoder.read_next_frame(&frame)) {
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_time - start_time;
        double elapsed_seconds = elapsed.count();
        double time_remaining = frame.timestamp_seconds - elapsed_seconds;
        if (time_remaining > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(time_remaining));
        }

//         std::cout << "Y Plane Size in bytes: " << frame.y_plane.size() << "\n";
    }

    return 0;
}

// Overload function for spesific frame to get
int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame, int desired_frame) {
    while (decoder.read_next_frame(&frame)) {
        if (frame.frame_index == desired_frame) {
//             std::cout << "Successfully isolated frame " << desired_frame << "!\n";
//             std::cout << "Y Plane Size in bytes: " << frame.y_plane.size() << "\n";
            break; // Stop decoding immediately, we found our frame.
        } else {
//             std::cout << "Not this one." << std::endl;
        }
    }
//     std::cout << "decode video done" << std::endl;
    return 1;
}

// Function to return the row stride size of a give width
// This function have duplicated part in various location, notably in shm.cpp and videoDecoder.hpp, but this is the most appropriate place to put it
size_t rowStrideSize(uint32_t width) {
    return (width * 4 + 63) & ~63ULL;
};

// Function to extract video's native height, width, and frame rate.
int extractVideoInfo(AVCodecContext* codec_ctx) {
    
}