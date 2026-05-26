// Main entry point to the program

#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <unistd.h>

#include "socketSend.hpp"
#include "socketReceive.hpp"
#include "tools.hpp"
#include "checkTerminal.hpp"
#include "DataType.hpp"

// Use:
// - PID → identify your process
// - window address → identify your exact window
// - parent process tree → detect terminal launch
// - Hyprland IPC → detect workspace/window state

int main() {
    // Check wether it was run via terminal or not
    if (checkTerminal() == 0) {
        std::cout << "Please rn via terminal";
        return 1;
    }

    // initialized the WindowData type
    WorkspaceData Data;

    // Check systme terminal emulator
    
    // No need actually, this is past uses
    // Get PID of the process
    // pid_t pid = getpid();
    
    // // Get current windows address from the pid, use socketsends
    // std::mutex socketSendMutex;
    // std::string windowsAddress;         

    // // Implementation of windwos title name terminal

    // // Change title name of the terminal
    // std::string TerminalWindowTittleName = ChangeWindowTittleName();
    
    // // I think theres some mismatch speed of the thread between the main and sockseSend's one but it still kinda works
    // int socketSend = GetWindowAddress(TerminalWindowTittleName, socketSendMutex, windowsAddress);
    
    // std::cout << "GetWindowAddress returned " << socketSend << " and address " << windowsAddress << "\n";

    // if (windowsAddress.empty()) {
    //     std::cerr << "No window address found for Window Tittle name " << TerminalWindowTittleName << ".\n";
    //     return 1;
    // }

    // // Get that window position and size
    // std::mutex socketSendMutex2;
    // std::vector<std::vector<int>> windowProperties;
    // int socketSend2 = GetWindowPos(windowsAddress, socketSendMutex2, windowProperties);
    // std::cout << "GetWindowPos returned " << socketSend2 << "\n";

    // if (!windowProperties.empty() && windowProperties.size() >= 2 && windowProperties[0].size() >= 2 && windowProperties[1].size() >= 2) {
    //     std::cout << "Position: " << windowProperties[0][0] << ", " << windowProperties[0][1] << std::endl;
    //     std::cout << "Size: " << windowProperties[1][0] << ", " << windowProperties[1][1] << std::endl;
    // } else {
    //     std::cerr << "No window property data found for address " << windowsAddress << ".\n";
    // }

    // Get all terminal window from that workspace
    // queryWorkspaceId();


    return 1;
}