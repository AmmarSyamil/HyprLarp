#include <iostream>
#include <vector>

#include "socket.hpp" // SocketConnection(data)

#include <thread> // multithreading!!

#include <string> // check letter to detect activewindow2

int main() {

    // Setup data and threading
    std::vector<std::string> data{};
    std::thread socketThread(SocketConnection, std::ref(data));

    std::string temp{};
    while (true) {
        while (data.size() > 0) {
    
            size_t i = data.size();
            
            temp = data.at(data.size()-1);
    
            for (i = 0; i <= 1; i--)
            { 
                if (temp.starts_with("activewindow2")) {
                    std::cout << "theres actice windows" << std::endl;
        
        
                } else {
                    data.erase(data.begin() + i);
                }
            }
            
    
            // Operation to filer out non "activewindow2"
            
    
    
    
        }
    }

    return 1;
}
