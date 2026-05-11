#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <cstring>
#include <iostream>

int main() {
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
    } else {
        std::cout << "Connection succesfull!!";
    };

    // Setup buffer with size 4096 bytes size (hopefully its enough)
    char buffer[4096];

    // Define data
    std::string data;

    // Read socket
    while (true) {
        int n = read(sock, buffer, sizeof(buffer)-1);


        buffer[n] = '\0';
        std::cout << "RECV: " << buffer << std::flush;

        // if (n>0) {
        //     data.append(buffer, n);

        //     // Output resilt
        //     std::cout << std::string(buffer, n);
        // }
    }
    
}