// File that contain AABS // any layouting of the video on terminal
// terminalLayout.cpp
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

// Main
void layoutCalculation(int v_x1, int v_y1, int v_x2, int v_y2,
                       InternalTerminalGeometry internalTerminalGeometry,
                       ViewportState& viewPort, WindowPos windowPos,
                       videoData videoData, LayoutRender& layoutRender) {

    GetOverlap(v_x1, v_y1, v_x2, v_y2, internalTerminalGeometry, viewPort);

    if (!viewPort.isRender) {
        return;
    }

    int video_rect_w = v_x2 - v_x1;
    int video_rect_h = v_y2 - v_y1;
    
    if (video_rect_w <= 0 || video_rect_h <= 0) {
        viewPort.isRender = false;
        return;
    }

    double scale_x = (double)videoData.video_w / (double)video_rect_w;
    double scale_y = (double)videoData.video_h / (double)video_rect_h;

    layoutRender.x = (int)((viewPort.overlap_x - v_x1) * scale_x);
    layoutRender.y = (int)((viewPort.overlap_y - v_y1) * scale_y);
    layoutRender.w = (int)(viewPort.overlap_w * scale_x);
    layoutRender.h = (int)(viewPort.overlap_h * scale_y);

    int local_pixel_x = viewPort.overlap_x - internalTerminalGeometry.grid_screen_x;
    int local_pixel_y = viewPort.overlap_y - internalTerminalGeometry.grid_screen_y;

    layoutRender.cursor_col = (local_pixel_x / internalTerminalGeometry.cell_w) + 1;
    layoutRender.cursor_row = (local_pixel_y / internalTerminalGeometry.cell_h) + 1;

    layoutRender.sub_offset_x = local_pixel_x % internalTerminalGeometry.cell_w;
    layoutRender.sub_offset_y = local_pixel_y % internalTerminalGeometry.cell_h;

    layoutRender.disp_cols = (viewPort.overlap_w + internalTerminalGeometry.cell_w - 1) / internalTerminalGeometry.cell_w;
    layoutRender.disp_rows = (viewPort.overlap_h + internalTerminalGeometry.cell_h - 1) / internalTerminalGeometry.cell_h;
}