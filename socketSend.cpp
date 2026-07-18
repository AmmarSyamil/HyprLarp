// This file contain the implementation of sending socket to Hyrpland IPC to find the propeties of the active window.
// socketSend.cpp

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <iostream>
#include <vector>
#include <list>
#include <mutex>
#include <nlohmann/json.hpp> 
#include "terminal.hpp"
#include "DataType.hpp"
#include <chrono>
#include <fstream>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>

#include "hyprlandIPC.hpp"

nlohmann::json convertSimdjsonToNlohmann(const simdjson::dom::element& el) {
    if (el.is_object()) {
        nlohmann::json obj;
        for (auto [key, value] : el.get_object()) {
            obj[std::string(key)] = convertSimdjsonToNlohmann(value);
        }
        return obj;
    } else if (el.is_array()) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto val : el.get_array()) {
            arr.push_back(convertSimdjsonToNlohmann(val));
        }
        return arr;
    } else if (el.is_string()) {
        return std::string(el.get_string().value());
    } else if (el.is_int64()) {
        return el.get_int64().value();
    } else if (el.is_bool()) {
        return el.get_bool().value();
    } else {
        return nlohmann::json();
    }
}

// Function that being called
static void hb(const char* where) {
    static std::ofstream dbg("/tmp/hyprlarp_heartbeat.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " " << where << std::endl;
    dbg.flush();
}

int connect_hyprland_socket(int sock, const std::string& path) {
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int conn = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (conn == 0) {
        // Connected immediately
        fcntl(sock, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS) {
        return -1;
    }

    struct pollfd pfd{ sock, POLLOUT, 0 };
    int pret = poll(&pfd, 1, 200);  // 200 ms timeout
    if (pret <= 0) {
        return -1;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0) {
        return -1;
    }

    fcntl(sock, F_SETFL, flags);
    return 0;
}

int send_and_receive_json(int sock, const std::string& cmd, std::string& response, int timeout_ms = 200) {
    // Send
    if (send(sock, cmd.c_str(), cmd.size(), MSG_NOSIGNAL) < 0) {  // ← MSG_NOSIGNAL
        return -1;
    }

    // Receive with poll
    char buffer[4096];
    response.clear();

    while (true) {
        struct pollfd pfd{ sock, POLLIN, 0 };
        int pret = poll(&pfd, 1, timeout_ms);
        if (pret <= 0) {
            return -1;   // timeout or error
        }

        ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // should not happen after poll
            return -1;
        }
        if (n == 0) {
            // peer closed, but we might have partial data? assume done
            break;
        }
        buffer[n] = '\0';
        response.append(buffer, n);

        // If we got less than the buffer size, assume the message is complete
        if (static_cast<size_t>(n) < sizeof(buffer) - 1) {
            break;
        }
    }

    return 0;
}

int GetHyprlandOption(const std::string& option, nlohmann::json& output) {
    simdjson::dom::element elem;
    if (HyprlandIPC::instance().getOption(option, elem) != 0) return 1;
    output = convertSimdjsonToNlohmann(elem);
    return 0;
}

int GetWindowsPropertiesData(nlohmann::json& outputData) {
    simdjson::dom::element elem;
    if (HyprlandIPC::instance().getClients(elem) != 0) return 1;
    outputData = convertSimdjsonToNlohmann(elem);
    return 0;
}

int queryPosWindow(const nlohmann::json& data, const std::string& address, WindowPos& output) {
    for (const auto& jsonData : data) {
        if (jsonData["address"] == address) {
            std::vector<int> at_vec = jsonData["at"].get<std::vector<int>>();
            std::vector<int> size_vec = jsonData["size"].get<std::vector<int>>();
            output.at = { at_vec[0], at_vec[1] };
            output.size = { size_vec[0], size_vec[1] };
            return 0;
        }
    }
    return 1;
}

int queryWorkspaceId(const nlohmann::json& data, std::string address) {
    for (const auto& jsonData : data) {
        if (jsonData["address"] == address) {
            return jsonData["workspace"]["id"];
        }
    }
    return -1;
}

int queryWindowAddress(const nlohmann::json& data, std::string address, std::string& output) {
    for (const auto& jsonData : data) {
        if (jsonData["title"] == address) {
            output = jsonData["address"].get<std::string>();
            return 0;
        }
    }
    return 1;
}

int queryWindowAddress(const nlohmann::json& data, int address, std::string& output) {
    for (const auto& jsonData : data) {
        if (jsonData["pid"] == address) {
            output = jsonData["address"].get<std::string>();
            return 0;
        }
    }
    return 1;
}

int GetWindowAddress(pid_t address, std::string& outputData, const nlohmann::json* data) {
    if (!data) {
        nlohmann::json localData;
        if (GetWindowsPropertiesData(localData) != 0) return 1;
        data = &localData;
    }
    return queryWindowAddress(*data, address, outputData);
}

int GetWindowAddress(std::string address, std::mutex& dataMutex, std::string& outputData, const nlohmann::json* data) {
    nlohmann::json localData;
    if (!data) {
        GetWindowsPropertiesData(localData);
        data = &localData;
    }
    std::lock_guard<std::mutex> lock(dataMutex);
    return queryWindowAddress(*data, address, outputData);
}


// int GetWindowsPropertiesData(nlohmann::json& outputData) {
//     // int sock = socket(AF_UNIX, SOCK_STREAM, 0);
//     // if (sock < 0) {
//     //     std::cerr << "Socket sends (.sock) connection failed at making sock\n";
//     //     return 1;
//     // }

//     // std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" + getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket.sock";

//     // hb("before_connect");
//     // if (connect_hyprland_socket(sock, path) != 0) {
//     //     hb("after_connect_error");
//     //     std::cerr << "GetWindowsPropertiesData: connect failed\n";
//     //     close(sock);
//     //     return 1;
//     // }
//     // hb("after_connect");

//     // std::string response;
//     // if (send_and_receive_json(sock, "j/clients", response, 200) != 0) {
//     //     std::cerr << "GetWindowsPropertiesData: send/recv failed\n";
//     //     close(sock);
//     //     return 1;
//     // }
//     // hb("after_recv");

//     // close(sock);

//     // if (response.empty()) {
//     //     std::cerr << "GetWindowsPropertiesData: empty response from Hyprland\n";
//     //     return 1;
//     // }

//     // try {
//     //     outputData = nlohmann::json::parse(response);
//     // } catch (const nlohmann::json::parse_error& e) {
//     //     std::cerr << "GetWindowsPropertiesData: JSON parse failed: " << e.what() << "\n";
//     //     return 1;
//     // }

//     // return 0;
//     return HyprlandIPC::instance().getClients(outputData);
// }

// Get all of window ID with the same workspaceID
nlohmann::json GetAllWindowOfaWorkspaceID(nlohmann::json data, int workspaceID) {
    nlohmann::json output;
    for (const auto& jsonData: data) {
        if (jsonData["workspace"]["id"] == workspaceID) {
            // std::cout << jsonData["title"] << std::endl;

            output.emplace_back(jsonData);
        }
    }

    // if  (output.empty()) {
    //     // return -1;
    //     // errror
    // }

    return output;
}