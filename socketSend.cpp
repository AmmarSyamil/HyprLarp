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
//Function that being called

static void hb(const char* where) {
    static std::ofstream dbg("/tmp/hyprlarp_heartbeat.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " " << where << std::endl;
    dbg.flush();
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

// Actually it called clients  { clients - lists all windows with their properties }
// Runs similarly wiht hyprctl clients -j
int GetWindowsPropertiesData(nlohmann::json& outputData) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock < 0) {
        std::cerr << "Socket sends (.sock) connection failed at making sock\n";
        return 1;
    }

    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000; // 200ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Get socket path
    std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" + getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket.sock";

    // Make the sockaddr_un stuff
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Connect to the socket
    hb("before_connect");
    int connection = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (connection < 0) {
        std::cerr << "Socket connection failed at connecting for socket sends";
        close(sock);
        return 1;
    }
    if (connection < 0) { // EINPROGRESS - wait for it to complete, bounded
        struct pollfd pfd{};
        pfd.fd = sock;
        pfd.events = POLLOUT;
        int pret = poll(&pfd, 1, 200); // 200ms bound, matching the recv timeout

        if (pret <= 0) {
            hb("after_connect_timeout");
            std::cerr << "GetWindowsPropertiesData: connect() timed out\n";
            close(sock);
            return 1;
        }

        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            hb("after_connect_error");
            std::cerr << "GetWindowsPropertiesData: connect() failed: " << strerror(so_error) << "\n";
            close(sock);
            return 1;
        }
    }
    hb("after_connect");

    // Restore stuff
    fcntl(sock, F_SETFL, flags);

    // Data to send
    std::string dataSend = "j/clients";
    int sendsConnection = send(sock, dataSend.c_str(), dataSend.size(), 0);
    if (sendsConnection < 0) {
        std::cerr << "Error at sending data socket";
        close(sock);
        return 1;
    }

    // Data for the result
    std::string jsonData{};
    char buffer[4096] = {0};

    // recv loop
    hb("before_recv");
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived < 0) {
            // Timed out (EAGAIN/EWOULDBLOCK) or a real error — either way,
            // don't try to parse a partial response.
            std::cerr << "GetWindowsPropertiesData: recv timed out or failed\n";
            close(sock);
            return 1;
        }
        if (bytesReceived == 0) {
            break; // peer closed, normal end of message
        }

        jsonData.append(buffer, static_cast<size_t>(bytesReceived));

        if (static_cast<size_t>(bytesReceived) < sizeof(buffer) - 1) {
            break;
        }
    }
    hb("after_recv");

    close(sock);
    if (jsonData.empty()) {
        std::cerr << "GetWindowsPropertiesData: empty response from Hyprland\n";
        return 1;
    }

    // Parse data — guard against malformed/partial JSON so a bad read
    // can't throw an uncaught exception out of the render loop.
    try {
        outputData = nlohmann::json::parse(jsonData);
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "GetWindowsPropertiesData: JSON parse failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}


// Function that called other function


// this function have no uses
// Maybe deprecate it later
// Using windows address to find window position and size
// int GetWindowPos(const std::string& address, std::mutex& dataMutex, std::vector<std::vector<int>>& outputData, nlohmann::json& data) {
    
//     dataMutex.lock();
//     int result = queryPosWindow(data, address, outputData);
//     dataMutex.unlock();

//     return result;
// }

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

// Testing grounds
// int main() {
//     std::string address{};
//     std::mutex dataMutex;
//     std::string outputData{};
//         std::vector<std::vector<int>> outputData2{};
        
//         int pid = 1810;
        
//         // address = "0x562adcc61790";
        
//         int test = SocketSendConnection(pid, dataMutex, outputData);
        
//         address = outputData;
//         //test
//         std::cout << outputData << std::endl;
        
//         int test2 = SocketSendConnection(address, dataMutex, outputData2);
        
//         if (!outputData2.empty() && outputData2.size() >= 2 && outputData2[0].size() >= 2 && outputData2[1].size() >= 2) {
//             std::cout << "Position: " << outputData2[0][0] << ", " << outputData2[0][1] << std::endl;
//             std::cout << "Size: " << outputData2[1][0] << ", " << outputData2[1][1] << std::endl;
//         }

//     return 1;
// }