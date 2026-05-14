// This file contain the implementation of socket read of the .socket2.sock socket to read what window is changing to pint point which window is the active window.

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

int SocketReceiveConnection(std::vector<std::string>& data, std::mutex& dataMutex) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock < 0) {
        std::cerr << "Socket conection failed at making sock\n";
        return 1;
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
        std::cout << connection;
        std::cout << '\n';
        std::cout << path;
        return 1;
    } else  {
        std::cout << "Connection succesfull!!";
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
        while ((pos = dataBuffer.find('\n')) != std::string::npos) {
            // Setup and lock mutex
            // std::mutex socketDataMutex;
            dataMutex.lock();

            std::string line = dataBuffer.substr(0, pos);

            // Operation to filer out non "activewindowv2"
            if (line.starts_with("activewindowv2")) {
                std::cout << "theres active windowv2" << std::endl;

                // Put the line in the data
                data.push_back(line);

                // Test file implementation
                test_file << line << '\n';
                test_file.flush();
            }

            dataBuffer.erase(0, pos+1);

            dataMutex.unlock(); // Unlock mutex
        }
    }

    if (n == 0) {
        std::cerr << "error" << std::endl;
    }

    if (!dataBuffer.empty()) {
        data.push_back(dataBuffer);
    }
    

    return 1;
}

// Testing grounds
int main() {
    std::vector<std::string> data{};
    std::mutex dataMutex;

    int test = SocketReceiveConnection(data, dataMutex);

    return 1;
}
