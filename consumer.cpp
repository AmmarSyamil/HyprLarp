#include <iostream>
#include "consumer.hpp"

//consumer.cpp
int main() {
    consumer cons;
    cons.displayImage();
    
    std::cout << "testing .." << std::endl;
    std::cin.get();
    return 0;
}