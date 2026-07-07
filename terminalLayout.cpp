// File that contain AABS // any layouting of the video on terminal

#include "DataType.hpp"
#include <string>


void GetOverlap(int v_x1, int v_y1, int v_x2, int v_y2, InternalTerminalGeometry internalTerminalGeometry, ViewportState& viewPort) {
    // Get inner grid boundaries
    int t_x1 = internalTerminalGeometry.grid_screen_x;
    int t_y1 = internalTerminalGeometry.grid_screen_y;
    int t_x2 = t_x1 + internalTerminalGeometry.w;
    int t_y2 = t_y1 + internalTerminalGeometry.h;

    // Perform AABS
    int o_x1 = std::max(t_x1, v_x1);
    int o_y1 = std::max(t_y1, v_y1);
    int o_x2 = std::min(t_x2, v_x2);
    int o_y2 = std::min(t_y2, v_y2);

    // Check if an intersection actually exists
    if (o_x2 > o_x1 && o_y2 > o_y1) {
        viewPort.isRender = true;
        viewPort.overlap_x = o_x1;
        viewPort.overlap_y = o_y1;
        viewPort.overlap_w = o_x2 - o_x1;
        viewPort.overlap_h = o_y2 - o_y1;
    } else {
        // No overlap; reset the state to safe zeros
        viewPort.isRender = false;
        viewPort.overlap_x = 0;
        viewPort.overlap_y = 0;
        viewPort.overlap_w = 0;
        viewPort.overlap_h = 0;
    }
}