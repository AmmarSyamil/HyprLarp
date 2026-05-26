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
    WorkspaceData Data;

    // Check systme terminal emulator
    
    // No need actually, this is past uses
    // Get PID of the process
    // pid_t pid = getpid();



    // Get all terminal window from that workspace
    Data.FetchWindowID();

    std::cout << Data;

    return 1;
}