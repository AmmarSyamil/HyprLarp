// Main entry point to the program

#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <unistd.h>

#include "socketSend.hpp"
#include "socketReceive.hpp"

// Use:
// - PID → identify your process
// - window address → identify your exact window
// - parent process tree → detect terminal launch
// - Hyprland IPC → detect workspace/window state

int main() {

    // Get PID of the process
    pid_t pid = getpid();
    
    // Get current windows address from the pid, use socketsends
    std::mutex socketSendMutex;
    std::string windowsAddress; 
    int socketSend = GetWindowAddress(pid, socketSendMutex, windowsAddress);
    std::cout << "GetWindowAddress returned " << socketSend << " and address '" << windowsAddress << "'\n";

    if (windowsAddress.empty()) {
        std::cerr << "No window address found for PID " << pid << ".\n";
        return 1;
    }

    // Get that window position and size
    std::mutex socketSendMutex2;
    std::vector<std::vector<int>> windowProperties;
    int socketSend2 = GetWindowPos(windowsAddress, socketSendMutex2, windowProperties);
    std::cout << "GetWindowPos returned " << socketSend2 << "\n";

    if (!windowProperties.empty() && windowProperties.size() >= 2 && windowProperties[0].size() >= 2 && windowProperties[1].size() >= 2) {
        std::cout << "Position: " << windowProperties[0][0] << ", " << windowProperties[0][1] << std::endl;
        std::cout << "Size: " << windowProperties[1][0] << ", " << windowProperties[1][1] << std::endl;
    } else {
        std::cerr << "No window property data found for address " << windowsAddress << ".\n";
    }

    return 1;
}