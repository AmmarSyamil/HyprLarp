#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <cstring>
#include <iostream>

#include <vector>
#include <list>

//making file
#include <fstream>


int SocketConnection(std::vector<std::string>& data) {
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

    // Setup buffer with size 1024 bytes size (hopefully its enough)
    char buffer[1024];

    // Define data
    std::string data;
    int n;
    
    // Define varialbe
    // std::vector<std::string> MainData{};
    std::string dataBuffer{};

    // Read socket
    while ((n = read(sock, buffer, sizeof(buffer)-1)) > 0) {
        // int n = read(sock, buffer, sizeof(buffer)-1);
        
        buffer[n] = '\0';
        std::cout << "RECV: " << buffer << std::flush;

        
        // data = buffer;
        dataBuffer.append(buffer, n);

        size_t pos;

        
        // get each line of it and save it as a list of the string eacch line.
        while ((pos = dataBuffer.find('\n')) != std::string::npos) {          
            
            std::string line = dataBuffer.substr(0, pos);
            data.push_back(line);
            

        }

    }

    if (n==0) {
        std::cerr << "error" << std::endl;
    }
}


// Testing grounds

int main() {
    std::vector<std::string> data{};
    int test = SocketConnection(data);

    // std::ofstream MyFile("test.txt");
    while (test > 0) {
        std::cout << data << std::endl;
        // MyFile << data;
    
    }
    if (test < 0) {
        std::cerr << "Error somewhere idk dont ask me" << std::endl;
    }
    // MyFile.close();

    
}