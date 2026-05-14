#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

int SocketSendConnection(std::string address, std::mutex& dataMutex, std::vector<std::vector<int>>& outputData);
int SocketSendConnection(int address, std::mutex& dataMutex, std::string& outputData);

int queryPosWindow(const nlohmann::json data, std::string address, std::vector<std::vector<int>>& output);
int queryWindowAddress(const nlohmann::json data, int address, std::string& output);
int queryWorkspaceID(const nlohmann::json data, int address, std::string& output);