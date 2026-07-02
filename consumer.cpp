#include <iostream>
#include <thread>
#include <chrono>
#include "consumer.hpp"
#include "shm.hpp"

// consumer.cpp
int main() {
    consumer cons;

    cons.setupSHMfileName(1); // Create SHM filename
    cons.setupSHM();    // Create and initialize the SHM  

    while (true) {
        cons.setupSHM();

        if (cons.populateSHM() == -1) {
            std::cerr << "consumer: failed to populate SHM" << std::endl;
            break;
        }

        if (cons.displayImage() == -1) {
            std::cerr << "consumer: failed to display image" << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

//     std::cout << "consumer: exiting" << std::endl;
    return 0;
}