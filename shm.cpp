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
#include <stdio.h>
#include <atomic>

extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "videoDecoder.hpp"


// Slot -> the 8 size buffer that dictate which frame is miaw

// Struct contain the info of a frame data
struct alignas(64) frameSlot {
    std::atomic<uint64_t> sequence; // sequence lock, odd = clean, even = dirty \\ further use
    // std::atomic<uint32_t> reader_count;
    uint32_t _pad[6];
};

//Header struct data
struct alignas(64) controlHeader {
    // Video properties
    uint32_t width;
    uint32_t height;
    uint32_t stride; // Padding for each frame
    uint32_t num_sloth; // Total size of the ring buffer

    // Producer broadcast state
    std::atomic<uint64_t> global_sequences; // Frame produced counter
    std::atomic<uint32_t> write_slot_index; // last Frame slot location [0 , 7]

    uint32_t _pad[2]; // Padding for after the metadata type shi

    frameSlot slotMetadata[8]; // Make frame slot = 8
};

// SHM datatype
// Contain all of the data
// Each page = 4096
struct shmData {
    controlHeader header;
    // Frame data per 4096 ish cuz its a page
    // Raw RGB file
};

// Function to returen a memory address of the frame of the slot index inputted
uint8_t* slotPtr(void* shmBase, int slot_index, size_t frame_bytes) {
    uint8_t* basePtr = static_cast<uint8_t*>(shmBase); // Memory pointer of the main program
    size_t headerPage = (sizeof(controlHeader) + 4095) / 4096;

    return basePtr + (headerPage * 4096) + (slot_index * 4096);
}


// For main SHM - Testing if pixel data is readable
int testSHM(int image_size, int width, int height, std::string shmFileName) {
    int fd = shm_open(shmFileName.c_str(), O_RDONLY, 0644);

    
    // When testing display SHMs, they have NO header (created with create_header=false)
    size_t total_size = image_size;  // SHM contains only pixel data
    
    
    void* ptr = mmap(NULL, total_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "testSHM : Failed to create SHM" << std::endl;
        close(fd);
        return -1;
    }
    
    uint8_t* pixelData = static_cast<uint8_t*>(ptr);  // No offset needed for display SHMs
    std::cout   << "First pixel byte: " << (int)pixelData[0] << std::endl;

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

// Function to open producer SHM / for consumer
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


// Create SHM
// NOTE: When create_header=false, creates SHM with ONLY pixel data (for consumer)
//       When create_header=true, creates SHM with VideoHeader + pixel data (for producer)
// For Consumer, we must NOT include header in display SHMs
// Returns pointer to pixel data area, skipping header if present
uint8_t* createSHM(int data_length, int width, int height, const std::string& SHMfilename, bool create_header) {
    // Validate inputs
    if (data_length <= 0 || width <= 0 || height <= 0) {
        std::cerr << "createSHM: Invalid dimensions - data_length=" << data_length 
                  << ", width=" << width << ", height=" << height << std::endl;
        return nullptr;
    }
    
    // For main SHM (producer), unlink old one first to ensure clean state
    // if (create_header && SHMfilename == "/vp_static") {
        // shm_unlink(SHMfilename.c_str());
        // std::cerr << "createSHM: Unlinked old " << SHMfilename << std::endl;
    // }

    // Unlink old SHM (if any leftover somehow)
    shm_unlink(SHMfilename.c_str());
    
    // Open the SHM
    int fd = shm_open(SHMfilename.c_str(), O_CREAT | O_RDWR, 0600); // 0600 = read write acces to current
    if (fd == -1) {
        std::cerr <<"Error at createSHM : failed to create/open shm" << std::endl;
        return nullptr;
    }
    
    // Change file size of the SHM to the required image size
    size_t total_size = (create_header ? sizeof(VideoHeader) : 0) + data_length;
    ftruncate(fd, total_size);

    // Put image file size (default) into the SHM and get the pointer to the SHM
    void* genericPtr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (genericPtr == MAP_FAILED) {
        std::cerr << "shm error" << std::endl;
        close(fd);
        return nullptr;
    }

    // Zero-initialize the entire SHM to prevent garbage data
    memset(genericPtr, 0, total_size);
    
    // For main SHM / producer
    if (create_header) {
        VideoHeader* header = static_cast<VideoHeader*>(genericPtr);
        header->width = width;
        header->height = height;
        header->image_size = data_length;
        
        std::cerr << "createSHM: Wrote header to " << SHMfilename << " - width=" << width 
                  << ", height=" << height << ", image_size=" << data_length << std::endl;
        
        close(fd);

        // Return pointer to pixel data area (skip header)
        return static_cast<uint8_t*>(genericPtr) + sizeof(VideoHeader);
    }
    
    close(fd);

    // For producer, return pointer to start (no header)
    return static_cast<uint8_t*>(genericPtr);
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

// Get image data from the main (producer) SHM (might change implementation due to dynamic video resolution)
std::vector<int> getImageSHM() {
    int fd = shm_open("/vp_static", O_RDONLY, 0644); // Open main SHM
    if (fd == -1) {
        std::cerr << "getImageSHM: Could not open SHM (from file descriptor)" << std::endl;
        return {0, 0, 0, 0};
    }
    
    struct stat dataStat;
    
    // Copy file to the struct
    if (fstat(fd, &dataStat) == -1) {
        std::cerr << "getImageSHM: fstat failed" << std::endl;
        close(fd);
        return {0, 0, 0, 0};
    }
    
    std::cerr << "getImageSHM: /vp_static size = " << dataStat.st_size << " bytes, VideoHeader size = " << sizeof(VideoHeader) << std::endl;
    
    if (dataStat.st_size < sizeof(VideoHeader)) {
        std::cerr << "getImageSHM: Shared memory is not initialized or too small" << std::endl;
        close(fd);
        return {0, 0, 0, 0};
    }

    size_t header_size = sizeof(VideoHeader);
    void* headerPtr = mmap(NULL, header_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (headerPtr == MAP_FAILED) {
        std::cerr << "getImageSHM: mmap failed" << std::endl;
        return {0, 0, 0, 0};
    }

    VideoHeader* data_ptr = static_cast<VideoHeader*>(headerPtr);
    
    int width = data_ptr->width;
    int height = data_ptr->height;
    int image_size = data_ptr->image_size;
    int videoHeaderSize = header_size;
    
    // Debug: print raw bytes
    std::cerr << "getImageSHM: Raw header bytes (first 12): ";
    uint8_t* raw = static_cast<uint8_t*>(headerPtr);
    for (int i = 0; i < 12 && i < header_size; i++) {
        fprintf(stderr, "%02x ", raw[i]);
    }
    fprintf(stderr, "\n");
    
    std::cerr << "getImageSHM: width=" << width << ", height=" << height << ", image_size=" << image_size << std::endl;

    munmap(headerPtr, header_size);
    return {width, height, image_size, videoHeaderSize};
}

// Function to delete the SHM
int deleteSHM() {
    shm_unlink("/vp_static");
    return 1;
};

// Function to put file into SHM
// Writing directly at offset 0 (no header offset needed)
int putSHM(uint8_t* shmPtr, const void* data, size_t data_size) {
    if (!shmPtr || !data_size) {
        std::cerr << "putSHM : SHM pointer invalid" << std::endl;
    }

    std::cout << "putSHM : pre memcpy" << std::endl;

    // Data written directly at offset 0 (Kitty reads from here)
    memcpy(shmPtr, data, data_size);

    std::cout << "putSHM : post memcpy" << std::endl;

    return 1;
}