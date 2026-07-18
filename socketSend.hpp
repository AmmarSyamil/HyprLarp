#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>
#include "DataType.hpp"
#include <simdjson.h>

int queryPosWindow(const nlohmann::json& data, const std::string& address, WindowPos& output);
int queryWorkspaceId(const nlohmann::json& data, std::string address);
int queryWindowAddress(const nlohmann::json& data, std::string address, std::string& output);
int queryWindowAddress(const nlohmann::json& data, int address, std::string& output);
int GetWindowsPropertiesData(nlohmann::json& outputData);
int GetHyprlandOption(const std::string& option, nlohmann::json& output);
nlohmann::json GetAllWindowOfaWorkspaceID(const nlohmann::json& data, int workspaceID);int connect_hyprland_socket(int sock, const std::string& path);
int send_and_receive_json(int sock, const std::string& cmd, std::string& response, int timeout_ms = 200);
int GetWindowAddress(pid_t address, std::string& outputData, const nlohmann::json* data = nullptr);
int GetWindowAddress(std::string address, std::mutex& dataMutex, std::string& outputData, const nlohmann::json* data = nullptr);