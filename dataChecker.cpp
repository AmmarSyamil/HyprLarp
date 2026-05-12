#include <iostream>
#include <vector>

#include "socket.hpp" // SocketConnection(data)

#include <thread> // multithreading!!

#include <string> // check letter to detect activewindow2

#include <mutex> // mutex for better multithreading

int main() {

    // Setup data and threading
    std::vector<std::string> data{};
    std::mutex dataMutex;
    std::thread socketThread(SocketConnection, std::ref(data), std::ref(dataMutex));

    std::string temp{};
    while (true) {
        while (data.size() > 0) {
    
            size_t i = data.size();
            
            temp = data.at(data.size()-1);
    
            for (i = 0; i <= 1; i--)
            { 

                dataMutex.lock();

                if (temp.starts_with("activewindow2")) {
                    std::cout << "theres actice windows" << std::endl;
        
        
                } else {
                    data.erase(data.begin() + i);
                }

                dataMutex.unlock();
            }
            
    
            // Operation to filer out non "activewindow2"
            
    
    
    
        }
    }

    return 1;
}
