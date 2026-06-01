// Function contain the implementation of all SHM
// shm.cpp
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "videoDecoder.hpp"

int testSHM(int image_size, int width, int height) {
    int fd = shm_open("/vp_static", O_RDONLY, 0644);

    
    size_t total_size = sizeof(VideoHeader) + image_size;
    
    
    void* ptr = mmap(NULL, total_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "testSHM : Failed to create SHM" << std::endl;
        close(fd);
        return -1;
    }
    
    uint8_t* pixelData = static_cast<uint8_t*>(ptr) + sizeof(VideoHeader);

    std::ofstream outFile("test_frame.ppm", std::ios::out | std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create test PPM file" << std::endl;
        munmap(ptr, image_size);
        close(fd);
        return -1;
    }

    // Write header file
    outFile << "P6\n"  << width << " " << height << "\n" << "255\n";

    // Write the data of the file
    for (int i = 0; i < image_size; i += 4) {
        outFile.write(reinterpret_cast<char*>(&pixelData[i]), 3);
    }

    outFile.close();
    munmap(ptr, total_size);

    close(fd);
    return 1;
} 

// Function to open SHM / for consumer
uint8_t* openSHM() {
    int fd = shm_open("/vp_static", O_RDWR, 0600); // 0600 = read write acces to current
    if (fd == -1) {
        std::cerr <<"Error at openSHM : SHM does not exist yet" << std::endl;
        return nullptr;
    }

    struct stat dataStat;
    int fs = fstat(fd, &dataStat);
    if (fs == -1){
        std::cerr << "openSHM error : cant do fstat" << std::endl;
        close(fd);
        return nullptr;
    }

    //Find data length : we can use Two-Stage Mapping method in the future
    if (dataStat.st_size == 0) {
        std::cerr << "openSHM error : Shared memory file is empty (0 bytes)." << std::endl;
        close(fd);
        return nullptr;
    }



    void* genericPtr = mmap(NULL, dataStat.st_size, PROT_READ , MAP_SHARED, fd, 0);
    close(fd);

    if (genericPtr == MAP_FAILED) {
     std::cerr << "shm error" << std::endl;
     close(fd);
     return nullptr;
    }

    return static_cast<uint8_t*>(genericPtr); // returning in uint8_t pointer form
}

uint8_t* createSHM(int data_length, int width, int height) {
    int fd = shm_open("/vp_static", O_CREAT | O_RDWR, 0600); // 0600 = read write acces to current
    if (fd == -1) {
        std::cerr <<"Error at createSHM : failed to create/open shm" << std::endl;
        return nullptr;
    }
    
    size_t total_size = sizeof(VideoHeader) + data_length;
    ftruncate(fd, total_size);
    void* genericPtr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (genericPtr == MAP_FAILED) {
        std::cerr << "shm error" << std::endl;
        close(fd);
        return nullptr;
    }

    VideoHeader* header = static_cast<VideoHeader*>(genericPtr);
    header->width = width;
    header->height = height;
    header->image_size = data_length;
    
    close(fd);
    return static_cast<uint8_t*>(genericPtr);;
}

// Function to unlink the SHM
// Must be call for both consumer and producer
int exitSHM(void* addr, int data_size) {

    // Un link shm
    if (addr) {
        munmap(addr, data_size);
    }
    return 1;

    // if (shm_unlink("/vp_static") == -1){
    //     return -1;
    // }; 

    return 1;
}



std::vector<int> getImageSHM() {
    int fd = shm_open("/vp_static", O_RDONLY, 0644);
    if (fd == -1) {
        std::cerr << "getImageSHM: Could not open SHM (from file descriptor)" << std::endl;
        return {0, 0, 0};
    }
    
    struct stat dataStat;
    if (fstat(fd, &dataStat) == -1 || dataStat.st_size < sizeof(VideoHeader)) {
        std::cerr << "getImageSHM : Shared memory is not initialize or too small" << std::endl;
        return {0, 0, 0};
    }

    size_t header_size = sizeof(VideoHeader);
    void* headerPtr = mmap(NULL, header_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (headerPtr == MAP_FAILED) {
        std::cerr << "getImageSHM : mmap failed" << std::endl;
        // close(fd);
        return {0 , 0, 0};
    }

    VideoHeader* data_ptr = static_cast<VideoHeader*>(headerPtr);
    
    int width = data_ptr->width;
    int height = data_ptr->height;
    int image_size = data_ptr->image_size;

    munmap(headerPtr, header_size);
    return {width, height, image_size};
}

// Function to delete the SHM
int deleteSHM() {
    shm_unlink("/vp_static");
    return 1;
};

// Function to put file into SHM
int putSHM(uint8_t* shmPtr,const void* data, size_t data_size) {
    if (!shmPtr || !data_size) {
        std::cerr << "putSHM : SHM pointer invalid" << std::endl;
    }

    // Header
    uint8_t* pixel_destination = shmPtr + sizeof(VideoHeader);

    std::cout << "putSHM : pre memcpy" << std::endl;

    // Data
    memcpy(pixel_destination, data, data_size); // cause memory error

    std::cout << "putSHM : post memcpy" << std::endl;


    return 1;
}