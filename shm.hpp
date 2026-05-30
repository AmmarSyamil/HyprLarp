#pragma once

uint8_t* createSHM(int data_length);
int exitSHM(void* addr, int data_size);
int putSHM(uint8_t* shmPtr, const void* data, size_t data_size);
