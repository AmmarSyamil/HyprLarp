#pragma once

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <mutex>

class HyprlandIPC{
private:
    HyprlandIPC();
    ~HyprlandIPC();
        
    int sock;
    bool peer_closed = false;
    std::string path;
    std::mutex mtx;

    bool ensureConnected();

public:    
    static HyprlandIPC& instance();

    // Returns 0 on success, 1 on failure
    int getClients(nlohmann::json& output);
    int getOption(const std::string& option, nlohmann::json& output);
    

};
