#pragma once

#include "DataType.hpp"
#include <sys/types.h>

// bool computeConsumerLayout(pid_t terminalPid, int video_w, int video_h, LayoutRender& layoutRender, ViewportState& viewPort);
bool computeConsumerLayout(pid_t terminalPid, int video_w, int video_h,
                           LayoutRender& layoutRender, ViewportState& viewPort,
                           InternalTerminalGeometry& geo);