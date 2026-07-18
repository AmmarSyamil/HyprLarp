#include "hyprlandIPC.hpp"
#include "socketSend.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#include <iostream>

HyprlandIPC::HyprlandIPC() : sock(-1) {
    const char* runtime = getenv("XDG_RUNTIME_DIR");
    const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!runtime || !sig) {
        std::cerr << "hyprlandIPC: HYPRLAND_INSTANCE_SIGNATURE not set\n";
        return;
    }
    path = std::string(runtime) + "/hypr/" + sig + "/.socket.sock";
}

HyprlandIPC::~HyprlandIPC() {
    if (sock >= 0) close(sock);
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
        std::cerr << "HyprlandIPC: socket creation failed\n";
        return false;
    }
    if (connect_hyprland_socket(sock, path) != 0) {
        std::cerr << "HyprlandIPC: connect failed to " << path << "\n";
        close(sock);
        sock = -1;
        return false;
    }
    return true;
}

int HyprlandIPC::getClients(simdjson::dom::element& output) {
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

    auto parse_result = parser.parse(response);
    if (parse_result.error()) {
        std::cerr << "HyprlandIPC: simdjson parse error: " << parse_result.error() << "\n";
        return 1;
    }
    cached_elem = parse_result.value();   // store the element
    output = cached_elem;
    return 0;
}

int HyprlandIPC::getOption(const std::string& option, simdjson::dom::element& output) {
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

    auto parse_result = parser.parse(response);
    if (parse_result.error()) {
        std::cerr << "HyprlandIPC: simdjson parse error for option: " << option << "\n";
        return 1;
    }
    cached_elem = parse_result.value();
    output = cached_elem;
    return 0;
}