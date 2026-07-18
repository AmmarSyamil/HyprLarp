#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#include <iostream>

#include "hyprlandIPC.hpp"
#include "socketSend.hpp"

HyprlandIPC::HyprlandIPC() : sock(-1) {
    const char* runtime = getenv("XDG_RUNTIME_DIR");
    const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!runtime || !sig) {
        std::cerr << "hyprlandIPC : HYPRLAND_INSTANCE_SIGNATURE not set" << std::endl;
        return;
    }

    path = std::string(runtime) + "/hypr/" + sig + "/.socket.sock";
}

HyprlandIPC::~HyprlandIPC() {
    if (sock >=0) close(sock);
}

HyprlandIPC& HyprlandIPC::instance() {
    static HyprlandIPC inst;
    return inst;
}

bool HyprlandIPC::ensureConnected() {
    if (sock >= 0 && !peer_closed) return true;
    if (sock >= 0) return true;
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "HyprlandIPC: socket creation failed" << std::endl;
        return false;
    }
    if (connect_hyprland_socket(sock, path) != 0) {
        std::cerr << "HyprlandIPC: connect failed to " << path << std::endl;
        close(sock);
        sock = -1;
        return false;
    }

    return true;
}


int HyprlandIPC::getClients(nlohmann::json &output) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!ensureConnected()) return 1;
    
    std::string response;
    if (send_and_receive_json(sock, "j/clients", response, 200) != 0) {
        close(sock);
        sock = -1;
        if (!ensureConnected()) return 1;
        if (send_and_receive_json(sock, "j/clients", response, 200) != 0) {
            close(sock);
            sock = -1;
            return 1;
        }
    }
    char peek;
    if (recv(sock, &peek, 1, MSG_DONTWAIT | MSG_PEEK) == 0) {
        peer_closed = true;   // ← detect early, skip wasted send next time
    }
    
    try {
        output = nlohmann::json::parse(response);
    } catch (...) {
        return 1;
    }

    return 0;
}

int HyprlandIPC::getOption(const std::string& option, nlohmann::json& output) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!ensureConnected()) return 1;

    std::string command = "j/getoption " + option;
    std::string response;
    if (send_and_receive_json(sock, command, response, 200) != 0) {
        close(sock);
        sock = -1;
        if (!ensureConnected()) return 1;
        if (send_and_receive_json(sock, command, response, 200) != 0) {
            close(sock);
            sock = -1;
            return 1;
        }
    }
    try {
        output = nlohmann::json::parse(response);
    } catch (...) {
        return 1;
    }
    return 0;
}

