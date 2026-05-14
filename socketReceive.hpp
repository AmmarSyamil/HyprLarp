#pragma once

#include <iostream>
#include <vector>
#include <mutex>


int SocketReceiveConnection(std::vector<std::string>& data, std::mutex& dataMutex);