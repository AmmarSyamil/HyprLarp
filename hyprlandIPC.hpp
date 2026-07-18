#pragma once

#include <string>
#include <mutex>
#include "simdjson.h"

class HyprlandIPC {
private:
    HyprlandIPC();
    ~HyprlandIPC();

    int sock;
    bool peer_closed = false;
    std::string path;
    std::mutex mtx;
    simdjson::dom::parser parser;
    simdjson::dom::element cached_elem;

    bool ensureConnected();

public:
    static HyprlandIPC& instance();
    int getClients(simdjson::dom::element& output);
    int getOption(const std::string& option, simdjson::dom::element& output);
};