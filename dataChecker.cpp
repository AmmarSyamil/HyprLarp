// Ignore this file, its funcionality already embeded in the socketReceive.cpp.

#include <iostream>
#include <vector>
#include "socketReceive.hpp" // SocketConnection(data)
#include <thread> // multithreading!!
#include <string> // check letter to detect activewindow2
#include <mutex> // mutex for better multithreading

int main() {

    // Setup data and threading
    std::vector<std::string> data{};
    std::mutex dataMutex;
    std::thread socketThread(SocketReceiveConnection, std::ref(data), std::ref(dataMutex));

    



    return 1;
}
