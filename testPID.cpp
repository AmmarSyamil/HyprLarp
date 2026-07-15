#include "checkTerminal.hpp"

int main() {
    int pid = FindTerminalPID();

    std::cout << pid << std::endl;

    return 0;
}