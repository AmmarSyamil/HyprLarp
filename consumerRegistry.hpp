#pragma once

#include <string>
#include <unistd.h>
#include <vector>

bool registerConsumer(pid_t pid);
bool unRegisterConsumer(pid_t pid);
bool checkConsumerState();