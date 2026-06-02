#include <iostream>
#include "consumer.hpp"
#include "shm.hpp"

//consumer.cpp
int main() {
    consumer cons;

    //Populate data of the consumer object
    // cons.getImageData(); I alreayd did that in constructur
    cons.setupSHMfileName();
    cons.setupSHM();
    cons.populateSHM();
    cons.displayImage();
    
    std::cout << "testing .." << std::endl;
    std::cin.get();
    return 0;
}