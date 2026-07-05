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
    

    // initialized the WindowData type
    // WorkspaceData Data;

    // // Check systme terminal emulator
    
    // // No need actually, this is past uses
    // // Get PID of the process
    // // pid_t pid = getpid();

    // // Get all terminal window from that workspace
    // std::cout << "initialize object done" << std::endl;
    // Data.FetchWindowID();
    // main terminal is mentioned twice
//     std::cout << "fetch window done" << std::endl;

//     std::cout << Data;

    WindowData data = WindowData(0);

    return 1;
}