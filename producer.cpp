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
#include "DataType.hpp"
#include <string>
#include "layoutSHM.hpp"
#include <cstring>
#include <chrono>


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

static void handle_signal(int) {
    shm_unlink("/HyprLarp-Producer");
    _exit(1);
}


void publishLayout(WorkspaceData& ws) {
    static LayoutHeader* hdr = nullptr;
    static int fd = -1;

    if (!hdr) {
        fd = shm_open("/HyprLarp_layout", O_CREAT | O_RDWR, 0600);
        if (fd == -1) {
            perror("publishLayout: shm_open");
            return;
        }
        if (ftruncate(fd, sizeof(LayoutHeader)) == -1) {
            perror("publishLayout: ftruncate");
            return;
        }
        hdr = (LayoutHeader*)mmap(nullptr, sizeof(LayoutHeader),
                                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);  // no longer needed after mmap
        if (hdr == MAP_FAILED) {
            perror("publishLayout: mmap");
            hdr = nullptr;
            return;
        }
        // Zero out the entire header on first mapping (ensures clean state)
        memset(hdr, 0, sizeof(LayoutHeader));
    }

    uint32_t i = 0;
    for (const auto& w : ws.getWindowData()) {
        if (i >= MAX_WINDOWS) break;  // safety

        const auto& lr = w.getLayoutRender();
        const auto& vp = w.getViewport();

        WindowLayoutEntry e{};
        std::strncpy(e.windowAddress, w.getWindowID().c_str(), sizeof(e.windowAddress) - 1);
        e.windowAddress[sizeof(e.windowAddress) - 1] = '\0';  // ensure null termination
        e.pid   = w.getPid();
        e.valid = 1;

        // Copy layoutRender fields
        e.x           = lr.x;
        e.y           = lr.y;
        e.w           = lr.w;
        e.h           = lr.h;
        e.cursor_col  = lr.cursor_col;
        e.cursor_row  = lr.cursor_row;
        e.disp_cols   = lr.disp_cols;
        e.disp_rows   = lr.disp_rows;
        e.sub_offset_x = lr.sub_offset_x;
        e.sub_offset_y = lr.sub_offset_y;

        // Copy ViewportState fields
        e.isRender   = vp.isRender ? 1 : 0;
        e.overlap_x  = vp.overlap_x;
        e.overlap_y  = vp.overlap_y;
        e.overlap_w  = vp.overlap_w;
        e.overlap_h  = vp.overlap_h;

        hdr->entries[i++] = e;
    }

    hdr->count.store(i, std::memory_order_release);
    hdr->version.fetch_add(1, std::memory_order_release);
}

int setupWorkspaceData() {
    // Setup workspace class
    WorkspaceData workspaceData;

    // Get windowData
    workspaceData.FetchWindowID();

    publishLayout(workspaceData);

    return 0;
};

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

    WorkspaceData workspaceData;
    workspaceData.FetchWindowID();
    publishLayout(workspaceData);

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
        
        static auto lastCheck = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastCheck > std::chrono::milliseconds(500)) {
            workspaceData.FetchWindowID();   // refresh window list
            publishLayout(workspaceData);
            lastCheck = now;
        }
    }

//     std::cout << "SHM deleted" << std::endl;
    deleteSHM();

    return 0;
}