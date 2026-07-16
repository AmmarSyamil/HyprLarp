//Use to check if the executable was run in terminal or not and what terminal is it
// checkTerminal.cpp
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <unordered_set>
#include <optional>

// return 1 if it run from terminal, return 0 if it not, and -1 if theres error
int checkTerminalWhichCurrentlyRunOn() {
    if (isatty(STDIN_FILENO)) {
//         std::cout << "Running from terminal\n";
        return 1;
    } else {
//         std::cout << "Not running from terminal\n";
        return 0;
    }


    return -1;
}


// Function to get the function process name from PID
std::string GetProcessName(pid_t pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/comm");

    std::string name;
    std::getline(file, name);

    return name;
}

#include <sstream>

pid_t GetParentPID(pid_t pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/stat");

    if (!file.is_open()) {
        return -1;
    }

    std::string line;
    std::getline(file, line);

    size_t endParen = line.rfind(')');

    if (endParen == std::string::npos) {
        return -1;
    }

    std::string after = line.substr(endParen + 2);

    std::istringstream iss(after);

    char state;
    pid_t ppid;

    iss >> state >> ppid;

    return ppid;
}


// Terminal emulator list
std::unordered_set<std::string> terminals = {
    "kitty",
    "alacritty",
    "wezterm",
    "foot",
    "ghostty",
    "konsole",
    "gnome-terminal-server",
    "xfce4-terminal",
    "tilix",
    "st"
};


// We can use this instead of the tittle??? it return the window PID
pid_t FindTerminalPID() {

    pid_t pid = getpid();

    while (pid > 1) {

        std::string name = GetProcessName(pid);

        if (terminals.contains(name)) {
            return pid;
        }

        pid = GetParentPID(pid);
    }

    return -1;
}


// Check wether the current procces run from kitty terminal or not
int isTerminalKitty() {
    pid_t pid = getpid();

    while (pid > 1) {
        std::string name = GetProcessName(pid);

        if ("kitty" == name) {
            return 1;
        }

        pid = GetParentPID(pid);
    }

    return -1;
}


// Is terminal return 1, not terminal return 0.
int IsPIDTerminal(pid_t pid) {
    while (pid > 1) {
        std::string name = GetProcessName(pid);

        if (terminals.contains(name)) {
            return 1;
        }

        pid = GetParentPID(pid);
    }

    return 0;
}

// int main() {
//     std::cout << FindTerminalPID() << std::endl;
// }