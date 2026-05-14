#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <unistd.h>

// Use:
// - PID → identify your process
// - window address → identify your exact window
// - parent process tree → detect terminal launch
// - Hyprland IPC → detect workspace/window state

int main() {

    // Get PID of the process
    pid_t pid = getpid();

    // Get current address from the pid, use socketsends

    


    return 1;
}