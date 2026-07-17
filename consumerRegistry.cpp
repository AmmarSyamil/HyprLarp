#include "consumerRegistry.hpp"
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

static const std::string REGISTRY_PATH = "/tmp/HyprLarp";

// Helper to read PIDs directly from the locked file descriptor
static std::vector<pid_t> read_registry_fd(int fd) {
    std::vector<pid_t> pids;
    // Reset file pointer to the beginning
    lseek(fd, 0, SEEK_SET);

    std::string content;
    char buf[256];
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, bytes_read);
    }

    std::stringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        try {
            pids.push_back(std::stoi(line));
        } catch (...) {}
    }
    return pids;
}

// Helper to write PIDs directly using the locked file descriptor
static bool write_registry_fd(int fd, const std::vector<pid_t>& pids) {
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) return false;
    if (ftruncate(fd, 0) != 0) return false;

    std::string content;
    for (pid_t p : pids) {
        content += std::to_string(p) + "\n";
    }

    ssize_t bytes_written = write(fd, content.data(), content.size());
    return bytes_written == static_cast<ssize_t>(content.size());
}

bool registerConsumer(pid_t pid) {
    int fd = open(REGISTRY_PATH.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) return false;

    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return false;
    }

    auto pids = read_registry_fd(fd);

    if (std::find(pids.begin(), pids.end(), pid) == pids.end()) {
        pids.push_back(pid);
    }

    bool ok = write_registry_fd(fd, pids);

    flock(fd, LOCK_UN);
    close(fd);
    return ok;
}

bool unRegisterConsumer(pid_t pid) {
    int fd = open(REGISTRY_PATH.c_str(), O_RDWR, 0666);
    if (fd < 0) return false;

    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return false;
    }

    auto pids = read_registry_fd(fd);
    auto it = std::find(pids.begin(), pids.end(), pid);
    
    bool ok = true;
    if (it != pids.end()) {
        pids.erase(it);
        ok = write_registry_fd(fd, pids);
    }

    flock(fd, LOCK_UN);
    close(fd);
    return ok;
}

bool checkConsumerState() {
    int fd = open(REGISTRY_PATH.c_str(), O_RDWR, 0666);   // O_RDWR, not O_RDONLY
    if (fd < 0) return false;

    if (flock(fd, LOCK_EX) != 0) {  
        close(fd);
        return false;
    }

    auto pids = read_registry_fd(fd);  
    std::vector<pid_t> alive;
    bool anyAlive = false;

    for (pid_t p : pids) {
        if (kill(p, 0) == 0) {
            alive.push_back(p);
            anyAlive = true;
        }
    }

    if (alive.size() != pids.size()) {
        ftruncate(fd, 0);
        lseek(fd, 0, SEEK_SET);
        for (pid_t p : alive) {
            std::string line = std::to_string(p) + "\n";
            write(fd, line.c_str(), line.size());
        }
    }

    flock(fd, LOCK_UN);
    close(fd);
    return anyAlive;
}
