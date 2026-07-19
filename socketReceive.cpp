// This file contain the implementation of socket read of the .socket2.sock socket to read what window is changing to pint point which window is the active window.
// socketRecieve.cpp

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <list>

// Function to connect to the socket and read the data from it, then filter out the activewindowv2 and put it in a list of string
int SocketReceiveConnection(std::vector<std::string>& data, std::mutex& dataMutex) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock < 0) {
        std::cerr << "Socket conection failed at making sock\n";
        return -1;
    }

    // Get socket path
    // $XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock
    std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "/hypr/" + getenv("HYPRLAND_INSTANCE_SIGNATURE") + "/.socket2.sock";

    // Make and setup the sockaddr_un struct
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());

    // Connect the socket
    int connection = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (connection < 0) {
        std::cerr << "Socket connection failed at conecting \n";
//         std::cout << connection;
//         std::cout << '\n';
//         std::cout << path;
        close(sock);
        return 1;
    } else  {
//         std::cout << "Connection succesfull!!";

    }

    // Setup buffer with size 1024 bytes size (hopefully its enough)
    char buffer[1024];

    // Define data
    int n;

    // Define varialbel
    std::string dataBuffer{};

    // Read socket
    while ((n = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        // std::cout << "RECV: " << buffer << std::flush;

        // data = buffer;
        dataBuffer.append(buffer, n);

        size_t pos;

        // Test file
        std::ofstream test_file("test.txt", std::ios::app);

        // get each line of it and save it as a list of the string eacch line.
        std::vector<std::string> batch;
        while ((pos = dataBuffer.find('\n')) != std::string::npos) {
            std::string line = dataBuffer.substr(0, pos);
            if (line.starts_with("activewindowv2")) {
                batch.push_back(std::move(line));   // avoid copies
            }
            dataBuffer.erase(0, pos + 1);
        }

        // Lock once and add all to the shared vector
        if (!batch.empty()) {
            std::lock_guard<std::mutex> lock(dataMutex);
            data.insert(data.end(),
                        std::make_move_iterator(batch.begin()),
                        std::make_move_iterator(batch.end()));
        }
    }

    if (n == 0) {
        std::cerr << "error" << std::endl;
    }

    if (!dataBuffer.empty()) {
        data.push_back(dataBuffer);
    }
    
    close(sock);
    return 1;
}

// Testing grounds
// int main() {
//     std::vector<std::string> data{};
//     std::mutex dataMutex;

//     int test = SocketReceiveConnection(data, dataMutex);

//     return 1;
// }
