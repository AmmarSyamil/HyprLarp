#pragma once

#include "DataType.hpp"


// int escSequence(int width, int height, std::string& imageSHM, const LayoutRender layoutRender, ViewportState vp);
int escSequence(int width, int height, const std::string& b64_shm, const LayoutRender& lr, const ViewportState& vp);
bool writeAll(int fd, const char* buf, size_t len);
