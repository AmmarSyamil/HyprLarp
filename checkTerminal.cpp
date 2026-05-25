//Use to check if the executable was run in terminal or not and what terminal is it

#include <iostream>
#include <unistd.h>

int checkTerminal() {
    if (isatty(STDIN_FILENO)) {
        std::cout << "Running from terminal\n";
        return 1;
    } else {
        std::cout << "Not running from terminal\n";
        return 0;
    }


    return 1;
}