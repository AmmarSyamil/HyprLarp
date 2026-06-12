#include <iostream>
#include "consumer.hpp"
#include "shm.hpp"

// consumer.cpp
int main() {
    // Setup consumer class
    consumer cons;

    cons.setupSHMfileName(1); // Create SHM filename
    cons.setupSHM();    // Create and initialize the SHM  
    cons.populateSHM(); // Populate the SHM with the imagefile
    cons.displayImage(); // Display the Image to the terminal usign kitty escape sequences
    
    std::cout << "testing .." << std::endl;
    std::cin.get();
    return 0;
}