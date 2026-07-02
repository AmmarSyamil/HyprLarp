#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

uint8_t* createSHM(int width, int height, const std::string& SHMfilename, bool create_header = true);
uint8_t* openSHM();
int exitSHM(void* addr, int data_size);
int putSHM(uint8_t* shmPtr, const void* data, size_t data_size);
int testSHM(int image_size, int width, int height, std::string shmFileName);
int deleteSHM();
std::vector<int> getImageSHM();
int writeFrameToSlot(void* shmPtr, int slot_index_target, const void* frame_data, int frame_number);


