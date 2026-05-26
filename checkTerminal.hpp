#pragma once
#include <iostream>


int checkTerminalWhichCurrentlyRunOn();

pid_t FindTerminalPID();

int IsPIDTerminal(pid_t pid);