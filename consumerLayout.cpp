#include "consumerLayout.hpp"
#include "socketSend.hpp"
#include "terminal.hpp"
#include "terminalLayout.hpp"
#include "simdjson.h"
#include <cstdlib>
#include <string>

static videoPos readVideoCorners() {
    simdjson::dom::parser parser;

    const char* homeDir = std::getenv("HOME");
    std::string filePath = homeDir
        ? std::string(homeDir) + "/.config/HyprLarp.json"
        : "HyprLarp.json";

    auto doc = parser.load(filePath);
    auto corners = doc["cornerLocation"];

    return {
        static_cast<int>(corners.at(0).at(0).get_int64().value()),
        static_cast<int>(corners.at(3).at(0).get_int64().value()),
        static_cast<int>(corners.at(0).at(1).get_int64().value()),
        static_cast<int>(corners.at(3).at(1).get_int64().value()),
    };
}

static WindowPos windowPosFromClient(const nlohmann::json& client) {
    return {
        client["at"].get<std::vector<int>>(),
        client["size"].get<std::vector<int>>()
    };
}

static bool lookupClientGeometryByPid(const nlohmann::json& data, pid_t terminalPid, WindowPos& pos) {
    bool found = false;

    for (const auto& client : data) {
        if (!client.contains("pid")) {
            continue;
        }
        if (client["pid"].get<pid_t>() != terminalPid) {
            continue;
        }

        pos = windowPosFromClient(client);
        found = true;
    }

    return found;
}

bool computeConsumerLayout(pid_t terminalPid, int video_w, int video_h,
                           LayoutRender& layoutRender, ViewportState& viewPort) {
    nlohmann::json clients;
    if (GetWindowsPropertiesData(clients) != 0) {
        return false;
    }

    WindowPos pos;
    if (!lookupClientGeometryByPid(clients, terminalPid, pos)) {
        return false;
    }

    InternalTerminalGeometry geo = GetInternalTerminalGeometry(pos);
    videoPos corners = readVideoCorners();

    videoData vd{};
    vd.video_w = video_w;
    vd.video_h = video_h;

    layoutCalculation(corners.video_left, corners.video_top,
                      corners.video_right, corners.video_bottom,
                      geo, viewPort, pos, vd, layoutRender);
    return true;
}
