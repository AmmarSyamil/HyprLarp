#pragma once
#include <cstdint>
#include <atomic>
#include <cstring>

constexpr int MAX_WINDOWS = 32;

// POD struct so it survives mmap cleanly
struct WindowLayoutEntry {
    char windowAddress[24]; // Hyprland address like "0x1234abcd"
    uint32_t pid;
    uint32_t valid;         // 0 = empty, 1 = valid

    // Layout render data
    int32_t x, y, w, h;
    int32_t cursor_col, cursor_row;
    int32_t disp_cols, disp_rows;
    int32_t sub_offset_x, sub_offset_y;

    // Viewport state
    uint32_t isRender;
    int32_t overlap_x, overlap_y, overlap_w, overlap_h;
};

struct alignas(64) LayoutHeader {
    std::atomic<uint64_t> version; // Bumped when layout changes
    std::atomic<uint32_t> count;   // Number of valid windows
    uint32_t _pad[14];
    WindowLayoutEntry entries[MAX_WINDOWS];
};