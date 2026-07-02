// Producer.cpp
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

int main() {
    // Setup VideoDecoder function
    VideoDecoder decoder;

    // Setup, create, and populate SHM metadata
    bool decode_open = decoder.open("video.mp4");

    if (!decode_open) {
//         std::cout << "failed" << std::endl;
        return -1;
    }

    // std::cout << "start" << std::endl;

    VideoFrameData frame;
    // int desired_frame = 1; 

    // Loop the video repeatedly
    while (true) {
        decodeVideo(decoder, frame);
        if (!decoder.rewind()) {
            std::cerr << "producer: failed to rewind video" << std::endl;
            break;
        }
    }

    deleteSHM();

//     std::cout << "SHM deleted" << std::endl;
    deleteSHM();

    return 0;
}

// gcc -g -o producer producer.cpp videoDecoder.hpp shm.cpp shm.hpp -lstdc++ -pthread -lavformat -lavcodec -lavutil -lswscale