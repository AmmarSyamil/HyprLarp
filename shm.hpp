#pragma once

uint8_t* createSHM(int data_length);
int exitSHM(void* addr, int data_size);