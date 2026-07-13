// consumerLayout.cpp
#include "consumerLayout.hpp"
#include "socketSend.hpp"
#include "terminal.hpp"
#include "terminalLayout.hpp"
#include "simdjson.h"
#include <cstdlib>
#include <fstream>

static videoPos readVideoCorners() {
    simdjson::dom::parser parser;
    const char* homeDir = std::getenv("HOME");
    std::string filePath = homeDir ? std::string(homeDir) + "/.config/HyprLarp.json" : "HyprLarp.json";
    auto doc = parser.load(filePath);
    auto corners = doc["cornerLocation"];
    return {
        static_cast<int>(corners.at(0).at(0).get_int64().value()),
        static_cast<int>(corners.at(3).at(0).get_int64().value()),
        static_cast<int>(corners.at(0).at(1).get_int64().value()),
        static_cast<int>(corners.at(3).at(1).get_int64().value())
    };
}

bool computeConsumerLayout(pid_t terminalPid, int video_w, int video_h, LayoutRender& layoutRender, ViewportState& viewPort) {
    nlohmann::json clients;
    if (GetWindowsPropertiesData(clients) != 0) return false;

    WindowPos pos;
    bool found = false;
    for (const auto& client : clients) {
        if (client.contains("pid") && client["pid"].get<pid_t>() == terminalPid) {
            pos.at   = client["at"].get<std::vector<int>>();
            pos.size = client["size"].get<std::vector<int>>();
            found = true;
            break;
        }
    }
    if (!found) return false;

    InternalTerminalGeometry geo = GetInternalTerminalGeometry(pos);  // uses ioctl on our own terminal

    videoPos corners = readVideoCorners();
    videoData vd{};
    vd.video_w = video_w;
    vd.video_h = video_h;

    

    layoutCalculation(corners.video_left, corners.video_top, corners.video_right, corners.video_bottom, geo, viewPort, pos, vd, layoutRender);

    std::ofstream dbg("/tmp/hyprlarp_debug.log", std::ios::app);
    dbg << "pid=" << terminalPid
        << " at=(" << pos.at[0] << "," << pos.at[1] << ")"
        << " size=(" << pos.size[0] << "," << pos.size[1] << ")"
        << " grid_w=" << geo.w << " grid_h=" << geo.h
        << " cols=" << geo.cols << " rows=" << geo.rows
        << " cell_w=" << geo.cell_w << " cell_h=" << geo.cell_h
        << " pad_x=" << geo.pad_x << " pad_y=" << geo.pad_y
        << " grid_screen_y=" << geo.grid_screen_y
        << std::endl;
    dbg << " isRender=" << viewPort.isRender
        << " overlap=(" << viewPort.overlap_x << "," << viewPort.overlap_y
        << "," << viewPort.overlap_w << "," << viewPort.overlap_h << ")"
        << std::endl;
    return true;
}