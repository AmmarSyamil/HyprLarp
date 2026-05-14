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
    int socketSend = SocketSendConnection(pid, socketSendMutex, windowsAddress);

    // Get workspace id
    int workspaceID;

    // int socketSendWorkspaceID = queryWorkspaceID()


    // Get that window position and size
    std::mutex socketSendMutex2;
    std::vector<std::vector<int>> windowProperties;
    int socketSend2 = SocketSendConnection(windowsAddress, socketSendMutex2, windowProperties);

    // Put it inside the layout

    return 1;
}