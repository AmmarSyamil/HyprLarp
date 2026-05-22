// File contain some basic functionality.

#include <iostream>
#include <random>
#include <string>
#include <chrono>
#include <thread>

std::string generate_random_string(std::size_t length) {
    const std::string characters = "01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string random_string;
    for (std::size_t i = 0; i < length; ++i) {
        random_string += characters[distribution(generator)];
    }

    return random_string;
}




std::string ChangeWindowTittleName() {

    std::string Tittle = generate_random_string(4);
    
    // Set tittle
    // we mabe can use     std::cout << "\033]21;" << Tittle << "\007" << std::flush; in future that are kitty spesific
    std::cout << "\033]0;"<< Tittle <<"\007" << std::flush;
    
    // Give terminal time to process the escape sequence
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return Tittle;
}



// int main() {
//     // std::cout << generate_random_string(5);
//     std::string TittleName = ChangeWindowTittleName();
//     std::cout << TittleName;
//     std::cin.get();
//     return 1;
// }