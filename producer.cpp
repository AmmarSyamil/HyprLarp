#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "videoDecoder.hpp"


// Function to output VideoFrameData
int decodeVideo(VideoDecoder decoder, VideoFrameData frame) {
    while (decoder.read_next_frame(&frame)) {
        std::cout << "Y Plane Size in bytes: " << frame.y_plane.size() << "\n";
    }

    return 0;
}

// Overload function for spesific frame to get
int decodeVideo(VideoDecoder decoder, VideoFrameData frame, int desired_frame) {
    while (decoder.read_next_frame(&frame)) {
        if (frame.frame_index == desired_frame) {
            std::cout << "Successfully isolated frame " << desired_frame << "!\n";
            std::cout << "Y Plane Size in bytes: " << frame.y_plane.size() << "\n";
            break; // Stop decoding immediately, we found our frame.
        } else {
            std::cout << "Not this one." << std::endl;
        }
    }
    
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

    return 0;
}