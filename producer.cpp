// Producer.cpp
#include "DataType.hpp"
#include <csignal>
#include <iostream>
#include <stdexcept>
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

#include "simdjson.h"

#include "videoDecoder.hpp"
#include "shm.hpp"

#include <string>

// Parse ~/.config/HyprLarp.json
std::string jsonParser() {
    simdjson::dom::parser parser;

    const char* homeDir = std::getenv("HOME");
    std::string filePath = homeDir
        ? std::string(homeDir) + "/.config/HyprLarp.json"
        : "HyprLarp.json";

    auto doc = parser.load(filePath);
    if (doc.error()) {
        throw std::runtime_error("Failed to load JSON config");
    }

    auto path = doc["videoPath"];
    if (path.error()) {
        throw std::runtime_error("videoPath not found");
    }

    return std::string(path.value().get_string().value());
}

int setupWorkspaceData() {
    // Setup workspace class
    WorkspaceData workspaceData;

    // Get windowData
    workspaceData.FetchWindowID();

    
};

static void handle_signal(int) {
    shm_unlink("/HyprLarp-Producer");
    _exit(1);
}

// Main enries to producer
int mainProducer() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    // WorkspaceData
    setupWorkspaceData();

    // Setup VideoDecoder function
    VideoDecoder decoder;

    // Setup, create, and populate SHM metadata
    bool decode_open = decoder.open(jsonParser());

    if (!decode_open) {
        std::cerr << "Producer : Failed to open the decode" << std::endl;
        throw std::runtime_error("Producer/decode : cant decode video");

        return -1;
    }

    VideoFrameData frame;
    // int desired_frame = 1; 

    // Loop the video repeatedly
    int loop_count = 0;
    while (true) {
        loop_count++;
        // std::cout << "[PRODUCER] === Loop " << loop_count << " ===" << std::endl;
        decodeVideo(decoder, frame);
        if (!decoder.rewind()) {
            std::cerr << "producer: failed to rewind video" << std::endl;
            throw std::runtime_error("Producer/decode : Failed at replaying the video");

            break;
        }
    }

//     std::cout << "SHM deleted" << std::endl;
    deleteSHM();

    return 0;
}