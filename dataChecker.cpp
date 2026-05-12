#include <iostream>
#include "socket.hpp"

int main() {

    std::string data{};
    if (int dataSocket = SocketConnection(data) < 0) {
        std::cerr << "Socker error from the datachecker file";
    }

    

}
