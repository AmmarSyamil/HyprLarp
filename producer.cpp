// Producer.cpp
#include <csignal>
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

static void handle_signal(int) {
    shm_unlink("/vp_static");
    _exit(1);
}

// idk header

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

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
    int loop_count = 0;
    while (true) {
        loop_count++;
        std::cerr << "[PRODUCER] === Loop " << loop_count << " ===" << std::endl;
        decodeVideo(decoder, frame);
        if (!decoder.rewind()) {
            std::cerr << "producer: failed to rewind video" << std::endl;
            break;
        }
    }

//     std::cout << "SHM deleted" << std::endl;
    deleteSHM();

    return 0;
}

// gcc -g -o producer producer.cpp videoDecoder.hpp shm.cpp shm.hpp -lstdc++ -pthread -lavformat -lavcodec -lavutil -lswscale