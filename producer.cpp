#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "videoDecoder.hpp"
#include "shm.hpp"

// idk header


// Function to output VideoFrameData
int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame) {
    while (decoder.read_next_frame(&frame)) {
        std::cout << "Y Plane Size in bytes: " << frame.y_plane.size() << "\n";
    }

    return 0;
}

// Overload function for spesific frame to get
int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame, int desired_frame) {
    while (decoder.read_next_frame(&frame)) {
        if (frame.frame_index == desired_frame) {
            std::cout << "Successfully isolated frame " << desired_frame << "!\n";
            std::cout << "Y Plane Size in bytes: " << frame.y_plane.size() << "\n";
            break; // Stop decoding immediately, we found our frame.
        } else {
            std::cout << "Not this one." << std::endl;
        }
    }
    std::cout << "decode video done" << std::endl;
    return 1;
}

int main() {
    VideoDecoder decoder;
    if (!decoder.open("video.mp4")) {
        std::cout << "failed" << std::endl;
        return -1;
    };

    std::cout << "start" << std::endl;

    VideoFrameData frame;
    int desired_frame = 1; 

    decodeVideo(decoder, frame, desired_frame);

    std::cout << decoder.numberofFrame() << std::endl;

    std::string t = "m";


    // Tester to consumer
    while (t == "m") {
        std::cin >> t;
    };

    deleteSHM();

    return 0;
}

// gcc -g -o producer producer.cpp videoDecoder.hpp shm.cpp shm.hpp -lstdc++ -pthread -lavformat -lavcodec -lavutil -lswscale