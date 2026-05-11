#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <cstring>
#include <iostream>

int main() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock < 0) {
        std::cerr << "Socket conection failed\n";
        return 1;
    }

    // Get socket path 
    // $XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock  
    std::string path = std::string(getenv("XDG_RUNTIME_DIR")) + "hypr" + std::string(getenv("HYPRLAND_INSTANCE_SIGNATURE")) + ".socket2.sock";


    // Make and setup the sockaddr_un struct
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    strcpy(addr.sun_path, path.c_str());

    // Connect the socket
    int connection = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (connection < 0) {
        std::cerr << "Socket connection failed \n";
        return 1;
    } else {
        std::cout << "Connection succesfull!!";
    };
    

}