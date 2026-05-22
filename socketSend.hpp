#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

// int SocketSendConnection(std::string address, std::mutex& dataMutex, std::vector<std::vector<int>>& outputData);
// int SocketSendConnection(int address, std::mutex& dataMutex, std::string& outputData);

int queryPosWindow(const nlohmann::json data, const std::string& address, std::vector<std::vector<int>>& output);
int queryWorkspaceId(const nlohmann::json data, int address, int& output);
int GetWindowsPropertiesData(nlohmann::json& outputData);
int GetWindowPos(const std::string& address, std::mutex& dataMutex, std::vector<std::vector<int>>& outputData, std::optional<nlohmann::json> data = std::nullopt);
int queryWindowAddress(const nlohmann::json data, int address, std::string& output);
int GetWindowAddress(pid_t address, std::mutex& dataMutex, std::string& outputData, std::optional<nlohmann::json> data = std::nullopt);
int GetWindowAddress(std::string address, std::mutex& dataMutex, std::string& outputData, std::optional<nlohmann::json> data = std::nullopt);