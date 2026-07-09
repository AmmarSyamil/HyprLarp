#pragma once

#include "DataType.hpp"

void GetOverlap(int v_x1, int v_y1, int v_x2, int v_y2, InternalTerminalGeometry internalTerminalGeometry, ViewportState& viewPort);
void layoutCalculation(int v_x1, int v_y1, int v_x2, int v_y2, InternalTerminalGeometry internalTerminalGeometry, ViewportState& viewPort, WindowPos windowPos, videoData videoData, LayoutRender& layoutRender);