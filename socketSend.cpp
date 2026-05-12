#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <iostream>

#include <vector>
#include <list>

#include <mutex>

int main() {
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
    std::string dataSend = "j/activewindow";

    // sends socket data
    int sendsConnection = send(sock, dataSend.c_str(), sizeof(addr), sizeof(dataSend));

    if (sendsConnection < 0) {
        std::cerr << "Error at sending data socket";
        
        return 1;
    }
    
    // Setup buffer with size 1024 bytes size (it sure enough)
    char buffer[1024]={0};

    // Receive data
    int bytesReceived = recv(sock, buffer, sizeof(buffer)-1, 0);

    // Check data received!!
    if (bytesReceived > 0) {
        std::cout << "Data received " << buffer << std::endl;

    } else if (bytesReceived == 0) {
        std::cerr << "Data received failed" << std::endl;

    } else {
        std::cerr << "Data received error" << std::endl;
    }
}