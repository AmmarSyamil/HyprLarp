// Function contain the implementation of all SHM
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>

extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

int testSHM(int image_size, int width, int height) {
    int fd = shm_open("/vp_static", O_CREAT | O_RDONLY, 0644);

    void* ptr = mmap(NULL, image_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Reader mmap failed!" << std::endl;
        return -1;
    }

    uint8_t* pixelData = static_cast<uint8_t*>(ptr);
    


    std::ofstream outFile("test_frame.ppm", std::ios::out | std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create test PPM file" << std::endl;
        munmap(ptr, image_size);
        return -1;
    }

    // Write header file
    outFile << "P6\n"  << width << " " << height << "\n" << "255\n";

    // Write the data of the file
    for (int i = 0; i < image_size; i += 4) {
        outFile.write(reinterpret_cast<char*>(&pixelData[i]), 3);
    }

    outFile.close();
    munmap(ptr, image_size);

    return 1;
} 

// Function to setup/create SHM
uint8_t* createSHM(int data_length, int width, int height) {
    int fd = shm_open("/vp_static", O_CREAT | O_RDWR, 0600); // 0600 = read write acces to current
    ftruncate(fd, data_length);
    void* genericPtr = mmap(NULL, data_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (genericPtr == MAP_FAILED) {
     std::cerr << "shm error" << std::endl;
     close(fd);
     return NULL;
    }

    
    // Casting general (void) pointer into uint8_t pointer
    uint8_t* shmPtr = static_cast<uint8_t*>(genericPtr);

    // // gng why we test on createSH=M ??
    // //Test SHM
    // testSHM(fd, data_length, width, height);

    if (shmPtr == nullptr) {
        std::cerr << "shm pointer general converter error" << std::endl;
        return NULL;
    }

    close(fd);
    return shmPtr;
}

// Function to remove the SHM
// Must be call for both consumer and producer
int exitSHM(void* addr, int data_size) {
    munmap(addr, data_size); // Un link shm
    shm_unlink("/vp_static"); // delete shm

    return 1;
}

// Function to put file into SHM
int putSHM(uint8_t* shmPtr,const void* data, size_t data_size) {
    memcpy(shmPtr, data, data_size);

    std::cout << "putSHM suki" << std::endl;

    return 1;
}