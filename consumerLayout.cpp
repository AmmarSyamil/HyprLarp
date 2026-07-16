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

bool computeConsumerLayout(pid_t terminalPid, int video_w, int video_h,
                           LayoutRender& layoutRender, ViewportState& viewPort,
                           InternalTerminalGeometry& geo) {
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

    // Compute geometry once and store it
    InternalTerminalGeometry geo_local = GetInternalTerminalGeometry(pos);
    geo = geo_local;   // copy to output parameter

    videoPos corners = readVideoCorners();
    videoData vd{};
    vd.video_w = video_w;
    vd.video_h = video_h;

    layoutCalculation(corners.video_left, corners.video_top,
                      corners.video_right, corners.video_bottom,
                      geo_local, viewPort, pos, vd, layoutRender);

    // (debug logging unchanged)
    return true;
}