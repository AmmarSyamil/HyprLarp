#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main() {
    VideoDecoder decoder;
    if (!decoder.open("input.mp4")) return -1;

    VideoFrameData target_frame;
    int desired_frame = 150; // Target the 150th frame

    while (decoder.read_next_frame(&target_frame)) {
        if (target_frame.frame_index == desired_frame) {
            std::cout << "Successfully isolated frame " << desired_frame << "!\n";
            std::cout << "Y Plane Size in bytes: " << target_frame.y_plane.size() << "\n";
            break; // Stop decoding immediately, we found our frame.
        }
    }

    return 0;
}