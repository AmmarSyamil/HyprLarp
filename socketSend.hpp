#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>
#include "DataType.hpp"

// int SocketSendConnection(std::string address, std::mutex& dataMutex, std::vector<std::vector<int>>& outputData);
// int SocketSendConnection(int address, std::mutex& dataMutex, std::string& outputData);

int queryPosWindow(const nlohmann::json data, const std::string& address, WindowPos& output);
int queryWorkspaceId(const nlohmann::json data, std::string address);
int GetWindowsPropertiesData(nlohmann::json& outputData);
int GetWindowPos(const std::string& address, std::mutex& dataMutex, std::vector<std::vector<int>>& outputData, std::optional<nlohmann::json> data = std::nullopt);
int queryWindowAddress(const nlohmann::json data, int address, std::string& output);
int GetWindowAddress(pid_t address, std::mutex& dataMutex, std::string& outputData, std::optional<nlohmann::json> data = std::nullopt);
int GetWindowAddress(std::string address, std::mutex& dataMutex, std::string& outputData, std::optional<nlohmann::json> data = std::nullopt);
nlohmann::json GetAllWindowOfaWorkspaceID(nlohmann::json data, int workspaceID);