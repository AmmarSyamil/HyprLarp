// Function to convert YUV420P to RGBA

#include <iostream>
#include <string>

#include "videoDecoder.hpp"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

uint8_t* dstData[4];
int dstLinesize[4];


int av_image_alloc();

int suli;

suli = sws_scale_frame();
