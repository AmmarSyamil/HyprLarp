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
        std::cout << "failed" << std::endl;
        return -1;
    }

    // std::cout << "start" << std::endl;

    VideoFrameData frame;
    // int desired_frame = 1; 

    // Decode video and copy to SHm
    decodeVideo(decoder, frame); 

    // std::cout << decoder.numberofFrame() << std::endl;
    
    // function to wait before finishing
    std::string t = "m";
    while (t == "m") {
        if (!(std::cin >> t)) {
            while (true) {
                sleep(2);
            }
        }
    }

    std::cout << "SHM deleted" << std::endl;
    deleteSHM();

    return 0;
}

// gcc -g -o producer producer.cpp videoDecoder.hpp shm.cpp shm.hpp -lstdc++ -pthread -lavformat -lavcodec -lavutil -lswscale