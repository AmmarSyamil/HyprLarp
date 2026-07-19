// producer.cpp
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
#include <fstream>
#include "checkTerminal.hpp"
#include "consumerRegistry.hpp"

#include <atomic>
#include <thread>


// Global flag to signal exit
std::atomic<bool> g_producer_should_exit{false};

static void monitor_consumers() {
    while (!g_producer_should_exit) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!checkConsumerState()) {
            g_producer_should_exit = true;
            break;
        }
    }
}

// universal logging
static void tlog(const char* tag, const std::string& msg) {
    static std::ofstream dbg("/tmp/hyprlarp_unified.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " [" << tag << ":" << FindTerminalPID() << "] " << msg << std::endl;
    dbg.flush();
}

static std::string get_video_path_from_config() {
    simdjson::dom::parser parser;
    const char* home = std::getenv("HOME");
    std::string filePath = std::string(home) + "/.config/HyprLarp.json";
    auto doc = parser.load(filePath);
    auto path = doc["videoPath"];
    return std::string(path.value().get_string().value());
}

static std::filesystem::file_time_type get_config_mod_time() {
    const char* home = std::getenv("HOME");
    std::string filePath = std::string(home) + "/.config/HyprLarp.json";
    return std::filesystem::last_write_time(filePath);
}

static bool decode_video_interruptible(VideoDecoder& decoder, VideoFrameData& frame) {
    auto start_time = std::chrono::high_resolution_clock::now();
    while (decoder.read_next_frame(&frame)) {
        // Check exit flag before each frame's sleep
        if (g_producer_should_exit.load()) return false; // interrupted

        auto current_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(current_time - start_time).count();
        double time_remaining = frame.timestamp_seconds - elapsed;
        if (time_remaining > 0) {
            // Sleep in small chunks to react to exit flag
            auto sleep_for = std::chrono::duration<double>(time_remaining);
            const auto chunk = std::chrono::milliseconds(50);
            while (sleep_for > chunk) {
                if (g_producer_should_exit.load()) return false;
                std::this_thread::sleep_for(chunk);
                sleep_for -= chunk;
            }
            std::this_thread::sleep_for(sleep_for);
        }

    }
    return true; 
}

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
    deleteSHM();
    // Corrected case-sensitivity to ensure cleanup works on termination signal
    shm_unlink("/Hyprlarp-Producer");
    _exit(1);
}


void publishLayout(WorkspaceData& ws) {
    static LayoutHeader* hdr = nullptr;
    static int fd = -1;
    
    // debug test impelemntatio
    if (!hdr) {
        fd = shm_open("/HyprLarp_layout", O_CREAT | O_RDWR, 0600);
        if (fd == -1) { perror("shm_open"); return; }
        if (ftruncate(fd, sizeof(LayoutHeader)) == -1) { perror("ftruncate"); return; }
        hdr = (LayoutHeader*)mmap(nullptr, sizeof(LayoutHeader),
                                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (hdr == MAP_FAILED) { hdr = nullptr; return; }
    }

    memset(hdr, 0, sizeof(LayoutHeader));


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

        // Debug test
        std::cerr << "Publishing window: " << e.windowAddress << " pid=" << e.pid << std::endl;
    }

    hdr->count.store(i, std::memory_order_release);
    hdr->version.fetch_add(1, std::memory_order_release);


}

int setupWorkspaceData() {
    // Setup workspace class
    WorkspaceData workspaceData;

    // Get windowData
    workspaceData.FetchWindowID();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    publishLayout(workspaceData);

    return 0;
};

// Main entries to producer
int mainProducer() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    // signal(SIGWINCH, handle_winch); // not needed but harmless

    std::string current_video_path = get_video_path_from_config();
    auto current_mod_time = get_config_mod_time();

    setupWorkspaceData();

    std::thread monitor(monitor_consumers);
    monitor.detach(); // runs independently

    VideoDecoder decoder;
    VideoFrameData frame;

    // FIX: Open the video file immediately on startup!
    if (!decoder.open(current_video_path)) {
        std::cerr << "[Producer] Failed to open video on startup: " << current_video_path << std::endl;
        return -1;
    }

    bool first_open = true;
    while (!g_producer_should_exit) {
        auto new_mod_time = get_config_mod_time();
        if (new_mod_time != current_mod_time) {
            std::string new_path = get_video_path_from_config();
            if (new_path != current_video_path) {
                decoder.close();
                if (decoder.open(new_path)) {
                    current_video_path = new_path;
                    current_mod_time = new_mod_time;
                    std::cerr << "[Producer] Switched to new video: " << new_path << std::endl;
                } else {
                    std::cerr << "[Producer] Failed to open new video, keeping old one." << std::endl;
                    decoder.open(current_video_path);
                    current_mod_time = new_mod_time; // avoid rechecking until next change
                }
            } else {
                current_mod_time = new_mod_time;
            }
        }

        bool completed = decode_video_interruptible(decoder, frame);
        if (g_producer_should_exit) break;

        if (!completed) {
            std::cerr << "[Producer] decode_video_interruptible failed, retrying..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (!decoder.rewind()) {
            std::cerr << "[Producer] Failed to rewind video, exiting." << std::endl;
            break;
        }
    }

    decoder.close();      
    deleteSHM();
    shm_unlink("/HyprLarp_layout");
    std::cerr << "[Producer] Exited cleanly." << std::endl;
    return 0;
}