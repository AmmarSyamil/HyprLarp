// Function contain the implementation of all SHM
// shm.cpp
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <thread>
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

static std::string normalizeSHMName(const std::string& name) {
    if (name.empty()) {
        return name;
    }
    if (name[0] == '/') {
        return name;
    }
    return std::string("/") + name;
}

// Function to returen a memory address of the frame of the slot index inputted
uint8_t* slotPtr(void* shmBase, int slot_index, size_t frame_bytes) {
    uint8_t* basePtr = static_cast<uint8_t*>(shmBase); // Memory pointer of the main program
    size_t headerPage = (sizeof(controlHeader) + 4095) / 4096;

    return basePtr + (headerPage * 4096) + (slot_index * frame_bytes);
}


// For main SHM - Testing if pixel data is readable
int testSHM(int image_size, int width, int height, std::string shmFileName) {
    shmFileName = normalizeSHMName(shmFileName);
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
//     std::cout   << "First pixel byte: " << (int)pixelData[0] << std::endl;

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
    int fd = shm_open(normalizeSHMName("/Hyprlarp-Producer").c_str(), O_RDWR, 0600); // 0600 = read write acces to current
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

    // Find data length : we can use Two-Stage Mapping method in the future
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

    // Check wether the SHM is initialized by the producer
    // If it isnt initialized, wait until it is
    controlHeader* header = static_cast<controlHeader*>(genericPtr);
    while (!header->initialized.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return static_cast<uint8_t*>(genericPtr); // returning in uint8_t pointer form
}


// Create SHM
// NOTE: When create_header=false, creates SHM with ONLY pixel data (for consumer)
//       When create_header=true, creates SHM with VideoHeader + pixel data (for producer)
// For Consumer, we must NOT include header in display SHMs
// Returns pointer to pixel data area, skipping header if present
uint8_t* createSHM(int width, int height, const std::string& SHMfilename, bool create_header) {
    // Validate inputs
    if (width <= 0 || height <= 0) {
        std::cerr << "createSHM: Invalid dimensions - data_length=" << ", width=" << width << ", height=" << height << std::endl;
        return nullptr;
    }
    
    std::string normalized_name = normalizeSHMName(SHMfilename);
    shm_unlink(normalized_name.c_str());
    
    int fd = shm_open(normalized_name.c_str(), O_CREAT | O_RDWR, 0600); // 0600 = read write acces to current
    if (fd == -1) {
        std::cerr <<"Error at createSHM : failed to create/open shm" << std::endl;
        return nullptr;
    }

    // Get SHM size
    size_t shm_size = 0;
    size_t stride = (width * 4 + 63) & ~63ULL;
    if (create_header) {
        size_t frame_size = stride * height;
        size_t header_size = (sizeof(controlHeader) + 4095) & ~4095ULL;

        shm_size = header_size + frame_size * RING_BUFFER_SLOTS;
    } else {
        // size_t stride = (width * 4 + 63) & ~63ULL;
        shm_size = stride * height;
    }
    
    // Change file size of the SHM to the required image size
    // size_t total_size = (create_header ? sizeof(VideoHeader) : 0) + data_length;
    // size_t total_size = (create_header ? sizeof(controlHeader) : 0) + data_length;
    ftruncate(fd, shm_size);

    // Put image file size (default) into the SHM and get the pointer to the SHM
    void* genericPtr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (genericPtr == MAP_FAILED) {
        std::cerr << "shm error" << std::endl;
        close(fd);
        return nullptr;
    }

    // Zero-initialize the entire SHM to prevent garbage data
    memset(genericPtr, 0, shm_size);
    
    // For main SHM / producer
    if (create_header) {

        // VideoHeader* header = static_cast<VideoHeader*>(genericPtr);
        controlHeader* header = static_cast<controlHeader*>(genericPtr);
        header->width = width;
        header->height = height;
        header->stride = stride; // Assuming RGBA format
        header->num_sloth = RING_BUFFER_SLOTS;
        // header->image_size = data_length;
        
        header->initialized.store(true, std::memory_order_release);
        std::cerr << "createSHM: Wrote header to " << SHMfilename << " - width=" << width  << ", height=" << height << std::endl;
        
        // close(fd);

        // Return pointer to pixel data area (skip header)
        // return static_cast<uint8_t*>(genericPtr) + sizeof(controlHeader);
    }
    
    close(fd);

    // Return generic pointer for both consumer and producer, use slotPtr() to get the correct pixel data pointer for consumers
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
}

// Get image data from the main (producer) SHM (might change implementation due to dynamic video resolution)
std::vector<int> getImageSHM() {
    int fd = shm_open("/Hyprlarp-Producer", O_RDONLY, 0644); // Open main SHM
    if (fd == -1) {
        std::cerr << "getImageSHM: Could not open SHM (from file descriptor)" << std::endl;
        return {0, 0, 0, 0};
    }
    
    struct stat dataStat;
    
    if (fstat(fd, &dataStat) == -1) {
        std::cerr << "getImageSHM: fstat failed" << std::endl;
        close(fd);
        return {0, 0, 0, 0};
    }
    
    size_t header_size = (sizeof(controlHeader) + 4095) & ~4095ULL;
    // std::cerr << "getImageSHM: /Hyprlarp-Producer size = " << dataStat.st_size << " bytes, controlHeader size = " << header_size << std::endl;
    
    if (dataStat.st_size < static_cast<off_t>(header_size)) {
        std::cerr << "getImageSHM: Shared memory is not initialized or too small" << std::endl;
        close(fd);
        return {0, 0, 0, 0};
    }

    void* headerPtr = mmap(NULL, header_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (headerPtr == MAP_FAILED) {
        std::cerr << "getImageSHM: mmap failed" << std::endl;
        return {0, 0, 0, 0};
    }

    controlHeader* data_ptr = static_cast<controlHeader*>(headerPtr);
    int width = data_ptr->width;
    int height = data_ptr->height;
    int image_size = data_ptr->stride * data_ptr->height;
    int videoHeaderSize = header_size;
    
    // std::cerr << "getImageSHM: Raw header bytes (first 12): ";
    // uint8_t* raw = static_cast<uint8_t*>(headerPtr);
    // for (int i = 0; i < 12 && i < static_cast<int>(header_size); i++) {
    //     // fprintf(stderr, "%02x ", raw[i]);
    // }

    // fprintf(stderr, "\n");
    
    // std::cerr << "getImageSHM: width=" << width << ", height=" << height << ", image_size=" << image_size << std::endl;

    munmap(headerPtr, header_size);
    return {width, height, image_size, videoHeaderSize};
}

// Function to delete the SHM
int deleteSHM() {
    shm_unlink(normalizeSHMName("/Hyprlarp-Producer").c_str());
    return 1;
};

// Function to put file into SHM
// Writing directly at offset 0 (no header offset needed)
int putSHM(uint8_t* shmPtr, const void* data, size_t data_size) {
    if (!shmPtr || !data_size) {
        std::cerr << "putSHM : SHM pointer invalid" << std::endl;
    }

//     std::cout << "putSHM : pre memcpy" << std::endl;

    // Data written directly at offset 0 (Kitty reads from here)
    memcpy(shmPtr, data, data_size);

//     std::cout << "putSHM : post memcpy" << std::endl;

    return 1;
}

// Alternatve to putSHM function
// Instead of writing to the SHM at offset 0, this function writes to a specific slot in the ring buffer
// Used for producer to write to a specific slot in the ring buffer
int writeFrameToSlot(void* shmPtr, int slot_index_target, const void* frame_data, int frame_number) {
    controlHeader* header = reinterpret_cast<controlHeader*>(shmPtr);

    uint8_t* dest = slotPtr(shmPtr, slot_index_target, header->stride * header->height);
    
    // change sequence to even to indicate dirty
    // header->slotMetadata[slot_index_target].sequence.fetch_add(static_cast<uint64_t>(frame_number) * 2ULL, std::memory_order_release); // Mark slot as dirty
    header->slotMetadata[slot_index_target].sequence.store(frame_number * 2, std::memory_order_release);

    std::memcpy(dest, frame_data, header->stride * header->height);

    // Change sequence to odd to indicate clean
    // header->slotMetadata[slot_index_target].sequence.fetch_add(1, std::memory_order_release); // Mark slot as clean
    header->slotMetadata[slot_index_target].sequence.store(frame_number * 2 + 1, std::memory_order_release);


    // Update the global sequence and write slot index
    header->global_sequences.store(frame_number, std::memory_order_release);
    header->write_slot_index.store(slot_index_target, std::memory_order_release);

    return 1;
}

// Function to read a frame from a specific slot in the ring buffer
int readFrameFromSlot(void* shmPtr, int slot_index_target, uint8_t* local_buffer) {
    controlHeader* header = reinterpret_cast<controlHeader*>(shmPtr);

    int data_size = header->stride * header->height;

    // Get sequence before reading
    uint64_t seq_before = header->slotMetadata[slot_index_target].sequence.load(std::memory_order_acquire);

    // Check if the slot is dirty (even sequence number)
    if (seq_before % 2 == 0) {
        std::cerr << "readFrameFromSlot: Slot " << slot_index_target << " is dirty, skipping read." << std::endl;
        return -1; // Indicate that the slot is dirty and cannot be read
    }

    // Get pointer to the slot data
    uint8_t* slotLocation = slotPtr(shmPtr, slot_index_target, data_size);

    // Copy data to local buffer
    std::memcpy(local_buffer, slotLocation, data_size);

    // Get sequence after reading
    uint64_t seq_after = header->slotMetadata[slot_index_target].sequence.load(std::memory_order_acquire);

    // Check if the sequence changed during the read
    if (seq_before != seq_after) {
        std::cerr << "readFrameFromSlot: Sequence changed during read for slot " << slot_index_target << ", data may be inconsistent." << std::endl;
        return -1;
    }

    return 1; // Successful read
};