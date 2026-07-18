// consumer.cpp
#include <thread>
#include <iostream>
#include <chrono>
#include "consumer.hpp"
#include "shm.hpp"
#include <csignal>
#include <chrono>
#include <fstream>
#include <csignal>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <atomic>

static volatile sig_atomic_t g_winch_flag = 0;
static std::atomic<pid_t> g_terminalPidForSignal{-1};


static void handle_winch(int) {
    g_winch_flag = 1;
}

// Handle signals to ensure the cursor is shown again on exit
static void handle_signal(int) {
    std::cout << "\033[?25h" << std::flush; // Show cursor
    const char* clear_seq = "\x1b_Ga=d,i=1\x1b\\";
    writeAll(STDOUT_FILENO, clear_seq, strlen(clear_seq));
    fflush(stdout);

    pid_t pid = g_terminalPidForSignal.load(std::memory_order_acquire);
    if (pid > 0) {
        unRegisterConsumer(pid); 
    }

    _exit(1);
}

// Handle refresh when theres change in window
bool consumer::checkHyprlandEvent() {
    static int sock = -1;
    if (sock == -1) {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) return false;
        std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" +
                           getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket2.sock";
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, path.c_str());
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            sock = -1;
            return false;
        }
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    // Check for pending data without blocking
    struct pollfd pfd{sock, POLLIN, 0};
    int ret = poll(&pfd, 1, 0);   // 0 timeout = immediate return
    if (ret > 0) {
        char buf[1024];
        ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n > 0) {
            return true;
        }
    }
    // If the socket is broken, close it so it re‑opens next time
    if (ret < 0 || (ret == 0 && errno == ECONNRESET)) {
        close(sock);
        sock = -1;
    }
    return false;
}

bool consumer::init() {
    if (getImageData() != 1) return false;

    ProducerSHMPtr = openSHM();
    if (!ProducerSHMPtr) return false;

    BaseSHMName = "HyprLarp_" + std::to_string(FindTerminalPID());

    terminalPid = FindTerminalPID();

    g_terminalPidForSignal.store(terminalPid, std::memory_order_release);
    registerConsumer(terminalPid); 

    return refreshLayout();
}

int consumer::renderFrame() {
    static int frameCounter = 0;
    bool needRefresh = false;
    if (g_winch_flag) {
        g_winch_flag = 0;          
        needRefresh = true;
    }

    if (checkHyprlandEvent()) {
        needRefresh = true;
    }
    
    if (++frameCounter % 60 == 0) {
        needRefresh = true;
    }

    if (needRefresh) {
        refreshLayout();         
    }

    // check rendering status
    static bool wasRendering = false;
    if (!viewPort.isRender) {
        if (wasRendering) {
            tlog("CONSUMER", "went_blank last_sequence=" + std::to_string(last_sequence));
            const char* clear_seq = "\x1b_Ga=d\x1b\\";
            // writeAll(STDOUT_FILENO, clear_seq, strlen(clear_seq));
            if (!writeAll(STDOUT_FILENO, clear_seq, strlen(clear_seq))) {
                std::cerr << "escSequence: writeAll failed" << std::endl;
                return 0;
            }
            fflush(stdout);
        }
        wasRendering = false;
        return 0;
    }
    if (!wasRendering) {
        last_sequence = static_cast<uint64_t>(-1);
        tlog("CONSUMER", "back_in_view resyncing_fresh");
    }
    wasRendering = true;
    
    uint64_t global_seq = 0;
    uint64_t target_seq = 0;
    controlHeader* header = reinterpret_cast<controlHeader*>(ProducerSHMPtr);
    if (!header) {
        logRenderFrameSkip("no_header", global_seq, target_seq);  // both 0, condition false
        return 0;
    }

    const uint32_t num_slots = header->num_sloth;
    if (num_slots == 0) {
        logRenderFrameSkip("num_slots_zero", global_seq, target_seq);
        return 0;
    }

    global_seq = header->global_sequences.load(std::memory_order_acquire);
    if (global_seq == 0) {
        logRenderFrameSkip("global_seq_zero", global_seq, target_seq);
        return 0;
    }

    // Compute target_seq
    if (last_sequence == static_cast<uint64_t>(-1)) {
        target_seq = global_seq;
    } else {
        target_seq = last_sequence + 1;
    }

    if (global_seq < target_seq) {
        logRenderFrameSkip("global_lt_target", global_seq, target_seq);
        return 0;
    }

    if (global_seq >= target_seq + num_slots) {
        target_seq = (global_seq > 2) ? global_seq - 2 : global_seq;
    }

    const uint32_t slot = static_cast<uint32_t>(target_seq % num_slots);
    const uint64_t expected_slot_seq = target_seq * 2 + 1;
    const uint64_t slot_seq = header->slotMetadata[slot].sequence.load(std::memory_order_acquire);

    if (slot_seq % 2 == 0) {
        logRenderFrameSkip("producer_writing", global_seq, target_seq);
        return 0;
    }
    if (slot_seq < expected_slot_seq) {
        logRenderFrameSkip("slot_seq_behind", global_seq, target_seq);
        return 0;
    }
    if (slot_seq > expected_slot_seq) {
        last_sequence = target_seq;
        logRenderFrameSkip("slot_seq_ahead_skip", global_seq, target_seq);
        return 0;
    }

    if (frame_data_cache.empty()) {
        frame_data_cache.resize(image_size);
    }
    if (readFrameFromSlot(ProducerSHMPtr, slot, frame_data_cache.data()) == -1) {
        logRenderFrameSkip("read_frame_failed", global_seq, target_seq);
        return 0;
    }

    const std::string frameSHMName = BaseSHMName + "_" + std::to_string(target_seq);
    const std::string frameB64Name = base64Converter(frameSHMName);

    hb("before_frame_shm_open");
    int fd = shm_open(("/" + frameSHMName).c_str(), O_CREAT | O_RDWR, 0600);
    if (fd == -1) { logRenderFrameSkip("shm_open_failed", global_seq, target_seq); return 0; }
    ftruncate(fd, image_size);

    uint8_t* consSHM = (uint8_t*)mmap(nullptr, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    hb("after_frame_shm_open");
    if (consSHM == MAP_FAILED) { logRenderFrameSkip("mmap_failed", global_seq, target_seq); return 0; }

    std::memcpy(consSHM, frame_data_cache.data(), image_size);

    escSequence(width, height, frameB64Name, layoutRender, viewPort);

    munmap(consSHM, image_size);
    if (!pendingKittyUnlink.empty()) {
        shm_unlink(("/" + pendingKittyUnlink).c_str());
    }
    pendingKittyUnlink = frameSHMName;
    last_sequence = target_seq;

    tlog("CONSUMER", "rendered target_seq=" + std::to_string(target_seq));
    return 1;
}

// Main entreies toward consumer
int mainConsumer() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    std::signal(SIGWINCH, handle_winch);

    consumer cons;

    // Hide cursor
    std::cout << "\033[?25l" << std::flush;

    // Cleaen the terminal
    std::cout << "\033[2J\033[H" << std::flush;

    // cons.setupSHMfileName(1); // Create SHM filename
    // cons.setupSHM();    // Create and initialize the SHM 

    // wait producer and layout to be ready 
    int attempts = 0;
    while (!cons.init()) {
        if (++attempts % 20 == 0)
            std::cerr << "Waiting for producer and layout...\n";
        if (attempts > 200) {   // 10 seconds
            std::cerr << "Timeout: producer failed to start. Exiting.\n";
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }


    while (true) {
        int result = cons.renderFrame();
        if (result == 1) {
            // Let Kitty finish reading the previous transmission SHM.
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        } else if (result == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Show cursor again
    std::cout << "\033[?25h" << std::flush;
    return 0;
}