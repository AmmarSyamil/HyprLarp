#include <iostream>
#include <thread>
#include <chrono>
#include "consumer.hpp"
#include "shm.hpp"

// consumer_debug.cpp - Display video + debug timing stats
int main() {
    consumer cons;

    cons.setupSHMfileName(1);       // Create SHM filename
    cons.setupSHM();                // Create and initialize the SHM
    cons.enableDebugMode();         // Enable debug stats

    while (true) {
        cons.setupSHM();            // Recreate display SHM each frame (Kitty unlinks it)

        if (cons.populateSHM() == -1) {
            break;
        }

        // Display video normally via Kitty
        if (cons.displayImage() == -1) {
            break;
        }

        // Print debug stats to stderr (doesn't interfere with video output)
        if (cons.displayDebugStats() == -1) {
            break;
        }

        cons.frame++;
        cons.setupSHMfileName(cons.frame);
    }

    return 0;
}
