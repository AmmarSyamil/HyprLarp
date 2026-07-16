// Main entry point to the program
// main.cpp


#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#include "gui.hpp"
#include "producer.hpp"
#include "consumer.hpp"
#include "checkTerminal.hpp"

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
        if (isTerminalKitty() == -1) {
            std::cerr << "Process currently run in non Kitty terminal, please try again using Kitty terminal." << std::endl;
            return -1;
        }
        
        // CLI
        if (argc > 1)
        {
            std::string arg = argv[1];
    
            if (arg == "--setting" or arg == "-s")
            {
                // Open GUI
                setup(argc, argv);
                return 0;
            }

            if (arg == "--producer" or arg == "-p") {
                mainProducer();
                return 0;
            }
    
            if (arg == "--help" or arg == "-h")
            {
                std::cout << "HyprLarp. Tools to Larp more efficiently." << std::endl;
                std::cout << "usage : HyprLarp [-h] [--gui]" << std::endl;
                std::cout << "" << std::endl;
                std::cout << "options:" << std::endl;
                std::cout << "  -h, --help      Show this message and exit" << std::endl;
                std::cout << "  -s, --setting   Launch configuration GUI tools to setup the config file (~/.config/HyprLarp.json)" << std::endl;
                std::cout << "  -p, --producer  Launch HyprLarp producer process only with their logs." << std::endl;
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
            std::cerr << "Failed to find config file, make sure " << configPath << " exists by running Hyprlarp --gui" << std::endl;
            return -1;
        }
    
        if (guard.is_first_instance()) {
            // Producer
            mainProducer();
            // mainConsumer();    
        } else {
            // Consumer
            mainConsumer();
        }
        
    } catch (const std::runtime_error& e) {
        std::cerr << "HyprLarp Failed to launch " << e.what() << std::endl;
    }

    return 1;
}