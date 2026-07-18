#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <vector>
#include <string>

inline constexpr uint32_t RING_BUFFER_SLOTS = 16;

struct alignas(64) frameSlot {
    std::atomic<uint64_t> sequence;
    std::atomic<uint32_t> reader_count;
    uint32_t _pad[13];
};

struct alignas(64) controlHeader {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t num_sloth;
    std::atomic<uint64_t> global_sequences;
    std::atomic<uint32_t> write_slot_index;
    std::atomic<bool> initialized = false;
    uint32_t _pad[2];
    frameSlot slotMetadata[RING_BUFFER_SLOTS];
};

uint8_t* createSHM(int width, int height, const std::string& SHMfilename, bool create_header = true);
uint8_t* openSHM();
int exitSHM(void* addr, int data_size);
int putSHM(uint8_t* shmPtr, const void* data, size_t data_size);
int testSHM(int image_size, int width, int height, std::string shmFileName);
int deleteSHM();
std::vector<int> getImageSHM();
int writeFrameToSlot(void* shmPtr, int slot_index_target, const void* frame_data, int frame_number);
int readFrameFromSlot(void* shmPtr, int slot_index_target, uint8_t* local_buffer);
int writeFrameToSlotStrided(void* shmPtr, int slot_index_target, const uint8_t* src, int src_stride, int frame_number);

