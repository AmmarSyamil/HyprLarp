#pragma once

#include <cstddef>
#include <cstdint>

uint8_t* createSHM(int data_length, int width, int height);
int exitSHM(void* addr, int data_size);
int putSHM(uint8_t* shmPtr, const void* data, size_t data_size);
int testSHM(int image_size, int width, int height);