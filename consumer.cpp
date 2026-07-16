// consumer.cpp
#include <thread>
#include <iostream>
#include <chrono>
#include "consumer.hpp"
#include "shm.hpp"
#include <csignal>
#include <chrono>
#include <fstream>


// Handle signals to ensure the cursor is shown again on exit
static void handle_signal(int) {
    std::cout << "\033[?25h" << std::flush; // Show cursor
    const char* clear_seq = "\x1b_Ga=d,i=1\x1b\\";
    writeAll(STDOUT_FILENO, clear_seq, strlen(clear_seq));
    fflush(stdout);
    _exit(1);
}

// Main entreies toward consumer
int mainConsumer() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    consumer cons;

    // Hide cursor
    std::cout << "\033[?25l" << std::flush;

    // Cleaen the terminal
    std::cout << "\033[2J\033[H" << std::flush;

    // cons.setupSHMfileName(1); // Create SHM filename
    // cons.setupSHM();    // Create and initialize the SHM 

    // wait producer and layout to be ready 
    while (!cons.init()) {
        static int wait_count = 0;
        if (++wait_count % 20 == 0)   // print every ~1 second
            std::cerr << "Waiting for producer and layout...\n";
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