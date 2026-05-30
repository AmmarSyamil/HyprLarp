// Function contain the implementation of all SHM
#include <iostream>
#include <algorithm>
#include <cstdint>

extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}


// Function to setup/create SHM
uint8_t* createSHM(int data_length) {
    int fd = shm_open("/vp_static", O_CREAT, 0600); // 0600 = read write acces to current
    ftruncate(fd, data_length);
    void* genericPtr = mmap(NULL, data_length, 0600, MAP_SHARED, fd, 0);

    if (genericPtr == MAP_FAILED) {
     std::cerr << "shm error" << std::endl;
     close(fd);
     return NULL;
    }

    // Casting general (void) pointer into uint8_t pointer
    uint8_t* shmPtr = static_cast<uint8_t*>(genericPtr);

    close(fd);
    return shmPtr;
}

// Function to remove the SHM
// Must be call for both consumer and producer
int exitSHM(void* addr, int data_size) {
    munmap(addr, data_size); // Un link shm
    shm_unlink("/video_frame_buffer"); // delete shm

    return 1;
}

// Function to put file into SHM
