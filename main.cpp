// Main entry point to the program
// main.cpp


#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "socketSend.hpp"
#include "socketReceive.hpp"
#include "tools.hpp"
#include "checkTerminal.hpp"
#include "DataType.hpp"
#include <fcntl.h>
#include <sys/file.h>

#include "gui.hpp"
#include "producer.hpp"
#include "consumer.hpp"

// Class to determine wether this instance will runned as consumer or producer
class InstanceManager{
private:
    int lock_fd;
    bool is_owner;
public:
    explicit InstanceManager(const std::string& app_name)
        : lock_fd(-1), is_owner(false) {

        // Lock file in /tmp/ , cleaned automatically on reboot
        std::string path = "/tmp/" + app_name + ".lock";

        // Open (or create) the lock file
        lock_fd = open(path.c_str(), O_CREAT | O_RDWR, 0666);
        if (lock_fd == -1) {
            // Can't open file // treat as non‑owner (safe fallback)
            return;
        }

        // Try to acquire an exclusive, non‑blocking lock
        if (flock(lock_fd, LOCK_EX | LOCK_NB) == 0) {
            // Lock acquired, this are the first instance
            is_owner = true;
        } else {
            // Lock failed, another instance is running
            close(lock_fd);
            lock_fd = -1;
            is_owner = false;
        }
    }

    ~InstanceManager() {
        if (lock_fd != -1) {
            close(lock_fd); // automatically releases the lock
        }
    }

    // True = first/only instance, False = subsequent instance
    bool is_first_instance() const {
        return is_owner;
    }

    // Move constructor/assignment can be deleted for safety
    InstanceManager(const InstanceManager&) = delete;
    InstanceManager& operator=(const InstanceManager&) = delete;
};


int main(int argc, char* argv[]) {
    try {
        InstanceManager guard("HyprLarp");
        
        // Check wether it was run via terminal or not
        
    
        // initialized the WindowData type
        // WorkspaceData Data;
    
        // // Check systme terminal emulator
        
        // // No need actually, this is past uses
        // // Get PID of the process
        // // pid_t pid = getpid();
    
        // // Get all terminal window from that workspace
        // std::cout << "initialize object done" << std::endl;
        // Data.FetchWindowID();
        // main terminal is mentioned twice
    //     std::cout << "fetch window done" << std::endl;
    
    //     std::cout << Data;
    
        // Create workspacedata
    
        // CLI
        if (argc > 1)
        {
            std::string arg = argv[1];
    
            if (arg == "--gui")
            {
                // Open GUI
                setup(argc, argv);
                return 0;
            }
    
            if (arg == "--help")
            {
                std::cout << "HyprLarp. Tools to Larp more efficiently." << std::endl;
                std::cout << "usage : HyprLarp [-h] [--gui]" << std::endl;
                std::cout << "" << std::endl;
                std::cout << "options:" << std::endl;
                std::cout << "  -h, --help      show this message and exit" << std::endl;
                std::cout << "  -g, --gui       Launch GUI setting to setup ~/.config/HyprLarp.json" << std::endl;
                return 0;
            }
        }
    
        // Run the program
        const char* homeDir = std::getenv("HOME");
        if (!homeDir) {
            std::cerr << "HOME environment variable not set" << std::endl;
            return -1;
        }

        std::filesystem::path configPath = std::filesystem::path(homeDir) / ".config/HyprLarp.json";
        if (!std::filesystem::exists(configPath)) {
            std::cerr << "Failed to find config file, make sure " << configPath
                    << " exists by running Hyprlarp --gui" << std::endl;
            return -1;
        }
    
        if (guard.is_first_instance()) {
            // Producer
            mainProducer();
    
        } else {
            // Consumer
            mainConsumer();
        }
        
    } catch (const std::runtime_error& e) {
        std::cerr << "HyprLarp Failed to launch " << e.what() << std::endl;
    }

    
    return 1;
}