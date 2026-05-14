// This file contain the implementation of sending socket to Hyrpland IPC to find the propeties of the active window.



#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <iostream>

#include <vector>
#include <list>

#include <mutex>

#include <nlohmann/json.hpp> 


int query(const nlohmann::json data, std::string address, std::vector<std::vector<int>>& output) {

    // nlohmann::json jsonData{};

    // std::cout << "query test" << std::endl;

    for (const auto& jsonData: data) {
        if (jsonData["address"] == address) {
            std::cout << jsonData["title"] << std::endl;

            output = {jsonData["at"], jsonData["size"]};

            //outputing for test for test
            // std::cout << "Position: " << output[0][0] << ", " << output[0][1] << std::endl;

            // std::cout << "Size: " << output[1][0] << ", " << output[1][1] << std::endl;
        }
    }
    
    return 1;
}



int SocketSendConnection(std::string address, std::mutex& dataMutex) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock < 0) {
        std::cerr << "Socket sends (.sock) connection failed at making sock\n";
        return 1;
    }

    // Get socket path
    // $XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket.sock  
    std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" + getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket.sock";

    // Make the sockaddr_un stuff
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    strcpy(addr.sun_path, path.c_str());

    //Connect to the socket
    int connection = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (connection < 0) {
        std::cerr << "Socket connection failed at connecting for socket sends";
        std::cout << connection << '\n' << path << std::endl;

        return 1;
    } else {
        std::cout << "Connection succesfull (socket sends)";
    }

    
    
    // Data to sends
    std::string dataSend = "j/clients";

    // sends socket data
    int sendsConnection = send(sock, dataSend.c_str(), sizeof(addr), 0);

    if (sendsConnection < 0) {
        std::cerr << "Error at sending data socket";
        return 1;
    }
    

    //Data for the result
    std::string jsonData{};


    // Setup buffer with size 4096 bytes size (it sure enough)
    char buffer[4096]={0};

    // Loop through the recv to get all data
    while (true) {
        int bytesReceived = recv(sock, buffer, sizeof(buffer)-1, 0);

        if (bytesReceived <= 0) {
            break;
        } 

        jsonData.append(buffer, bytesReceived);

        if (bytesReceived < sizeof(buffer)) {
            break;
        }
    }


    // Parse data
    nlohmann::json parsedData = nlohmann::json::parse(jsonData);

    // Debug
    // std::cout << '\n'<< parsedData << std::endl;

    // Setup output data
    std::vector<std::vector<int>> outputData;

    dataMutex.lock();

    // Check windows json
    query(parsedData, address, outputData);

    dataMutex.unlock();

    // Get data
    if (!outputData.empty()) {
        std::cout << "Position: " << outputData[0][0] << ", " << outputData[0][1] << std::endl;
        std::cout << "Size: " << outputData[1][0] << ", " << outputData[1][1] << std::endl;
    }

    return 1;
}

// Testing grounds
int main() {
    std::string address{};
    std::mutex dataMutex;

    address = "0x562adcc61790";

    int test = SocketSendConnection(address, dataMutex);

    return 1;
}

