#include <iostream>
#include <thread>
#include <chrono>
#include "consumer.hpp"
#include "shm.hpp"
#include <csignal>

// Handle signals to ensure the cursor is shown again on exit
static void handle_signal(int) {
    std::cout << "\033[?25h" << std::flush; // Show cursor again
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    while (true) {
        if (cons.renderFrame() != 1) {
            // Optional: check if layout changed, re‑fetch, etc.
        }
    }

    // while (true) {
    //     cons.setupSHM();

    //     if (cons.populateSHM() == -1) {
    //         // std::cerr << "consumer: failed to populate SHM" << std::endl;
    //         throw std::runtime_error("Cannot divide by zero!");
    //         break;
    //     }

    //     if (cons.displayImage() == -1) {
    //         // std::cerr << "consumer: failed to display image" << std::endl;
    //         throw std::runtime_error("Cannot divide by zero!");
    //         break;
    //     }

    //     cons.frame++;
    //     cons.setupSHMfileName(cons.frame);
    // }

    // Show cursor again
    std::cout << "\033[?25h" << std::flush;
    return 0;
}