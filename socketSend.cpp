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
    // int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    // if (sock < 0) {
    //     std::cerr << "GetHyprlandOption: socket creation failed\n";
    //     return 1;
    // }

    // std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" + getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket.sock";

    // if (connect_hyprland_socket(sock, path) != 0) {
    //     std::cerr << "GetHyprlandOption: connect failed\n";
    //     close(sock);
    //     return 1;
    // }

    // std::string command = "j/getoption " + option;
    // std::string response;
    // if (send_and_receive_json(sock, command, response, 200) != 0) {
    //     std::cerr << "GetHyprlandOption: send/recv failed\n";
    //     close(sock);
    //     return 1;
    // }

    // close(sock);

    // try {
    //     output = nlohmann::json::parse(response);
    // } catch (const nlohmann::json::parse_error& e) {
    //     std::cerr << "GetHyprlandOption: JSON parse failed: " << e.what() << "\n";
    //     return 1;
    // }

    // return 0;
    return HyprlandIPC::instance().getOption(option, output);
}

int GetWindowsPropertiesData(nlohmann::json& outputData) {
    // int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    // if (sock < 0) {
    //     std::cerr << "Socket sends (.sock) connection failed at making sock\n";
    //     return 1;
    // }

    // std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" + getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket.sock";

    // hb("before_connect");
    // if (connect_hyprland_socket(sock, path) != 0) {
    //     hb("after_connect_error");
    //     std::cerr << "GetWindowsPropertiesData: connect failed\n";
    //     close(sock);
    //     return 1;
    // }
    // hb("after_connect");

    // std::string response;
    // if (send_and_receive_json(sock, "j/clients", response, 200) != 0) {
    //     std::cerr << "GetWindowsPropertiesData: send/recv failed\n";
    //     close(sock);
    //     return 1;
    // }
    // hb("after_recv");

    // close(sock);

    // if (response.empty()) {
    //     std::cerr << "GetWindowsPropertiesData: empty response from Hyprland\n";
    //     return 1;
    // }

    // try {
    //     outputData = nlohmann::json::parse(response);
    // } catch (const nlohmann::json::parse_error& e) {
    //     std::cerr << "GetWindowsPropertiesData: JSON parse failed: " << e.what() << "\n";
    //     return 1;
    // }

    // return 0;
    return HyprlandIPC::instance().getClients(outputData);
}


//Function to get the window address from the PID of the process
int queryPosWindow(const nlohmann::json data, const std::string& address, WindowPos& output) {

    // well we can do this std::vector<int> at = jsonData["at"].get<std::vector<int>>();
    for (const auto& jsonData: data) {
        if (jsonData["address"] == address) {
            // std::cout << "Found window title: " << jsonData["title"] << std::endl;
            // output = {jsonData["at"], jsonData["size"]};

            output.at = jsonData["at"].get<std::vector<int>>();
            output.size = jsonData["size"].get<std::vector<int>>();

            return 0;
        }
    }

    // if (!found) {
    //     std::cerr << "queryPosWindow: no matching address found for " << address << std::endl;
    // }

    return 1;
}

// Overload function of the query for the PID way of input
// Output address of the window from input PID
int queryWindowAddress(const nlohmann::json& data, int address, std::string& output) {
    bool found = false;

    for (const auto& jsonData: data) {
        if (jsonData["pid"] == address) {
//             std::cout << "Found PID window title: " << jsonData["title"] << std::endl;
            output = jsonData["address"].get<std::string>();
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "queryWindowAddress: no matching pid found for " << address << std::endl;
    }

    return found ? 0 : 1;
}

//Overload from terminal window tittle name
// Output address of the window from input tittle name
int queryWindowAddress(const nlohmann::json& data, std::string address, std::string& output) {
    bool found = false;

    for (const auto& jsonData: data) {
        if (jsonData["title"] == address) {
//             std::cout << "Found window: " << jsonData["title"] << std::endl;
            output = jsonData["address"].get<std::string>();
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "queryWindowAddress: no matching window found for " << address << std::endl;
    }

    return found ? 0 : 1;
}

// Function to return the Workspace ID from given window address
int queryWorkspaceId(const nlohmann::json data, std::string address) {

    // nlohmann::json jsonData{};

    // std::cout << "query test" << std::endl;

    for (const auto& jsonData: data) {
        if (jsonData["address"] == address) {
//             std::cout << jsonData["title"] << std::endl;

            return jsonData["workspace"]["id"];
        }
    }
    
    return -1;
}

// Using PID address to find windows address
int GetWindowAddress(pid_t address, std::string& outputData, const nlohmann::json* data = nullptr) {
    if (!data) {
        nlohmann::json jsonData;
        if (GetWindowsPropertiesData(jsonData) != 0) {
            return 1;
        }
        data = &jsonData;
    }

    // dataMutex.lock();
    int result = queryWindowAddress(*data, address, outputData);
    // dataMutex.unlock();

    return result;
}

//Function overload for windows tittle terminal
int GetWindowAddress(std::string address, std::mutex& dataMutex, std::string& outputData, const nlohmann::json* data = nullptr) {
    nlohmann::json localData;
    if (!data) {
        GetWindowsPropertiesData(localData);
        data = &localData;
    }

    // Use this wierd add mutex
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        int result = queryWindowAddress(*data, address, outputData);
        
        return result;
    };
    // dataMutex.lock();
    // dataMutex.unlock();
    // std::lock_guard<std::mutex> 
    return 1;
}


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