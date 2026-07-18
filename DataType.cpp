// File of the project data type
// DataType.cpp
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <list>
#include <unistd.h>
#include "DataType.hpp"
#include "socketSend.hpp"
#include "tools.hpp"
#include "layoutWindow.hpp"
#include "checkTerminal.hpp"
#include <unordered_set>
#include "terminal.hpp"
#include "simdjson.h"
#include <string_view>
#include <cstdlib>
#include "terminalLayout.hpp"
#include <unordered_map>

extern "C" {
    #include <libavformat/avformat.h>
}

#include <mutex>
#include <optional>

static std::optional<videoData> g_cachedVideoData;
static std::string g_cachedVideoPath;
static std::mutex g_videoCacheMutex;

// Cached accessor – reads metadata only once per video file path
static const videoData& getCachedVideoData(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_videoCacheMutex);

    if (g_cachedVideoData && g_cachedVideoPath == path) {
        return *g_cachedVideoData;
    }

    // Otherwise open the file and read metadata
    videoData vd;
    vd.videoPath = path;

    AVFormatContext* format_ctx = nullptr;
    if (avformat_open_input(&format_ctx, path.c_str(), nullptr, nullptr) != 0) {
        throw std::runtime_error("Failed to open video: " + path);
    }
    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        avformat_close_input(&format_ctx);
        throw std::runtime_error("Failed to find stream info for: " + path);
    }

    bool found_video = false;
    for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVCodecParameters* codec_params = format_ctx->streams[i]->codecpar;
            AVStream* stream = format_ctx->streams[i];
            vd.video_w = codec_params->width;
            vd.video_h = codec_params->height;
            if (stream->avg_frame_rate.den > 0) {
                vd.fps = static_cast<double>(stream->avg_frame_rate.num) /
                         stream->avg_frame_rate.den;
            }
            found_video = true;
            break;
        }
    }
    avformat_close_input(&format_ctx);

    if (!found_video) {
        throw std::runtime_error("No video stream found in: " + path);
    }

    g_cachedVideoData = vd;
    g_cachedVideoPath = path;
    return *g_cachedVideoData;
}

static WindowPos windowPosFromClient(const nlohmann::json& client) {
    WindowPos pos;
    std::vector<int> at_vec = client["at"].get<std::vector<int>>();
    std::vector<int> size_vec = client["size"].get<std::vector<int>>();
    // Assume exactly 2 elements (Hyprland always returns [x, y])
    pos.at = { at_vec[0], at_vec[1] };
    pos.size = { size_vec[0], size_vec[1] };
    return pos;


// Prefer the last matching client entry; Hyprland can briefly emit duplicates
// during tiling animations and the later entry is usually the settled one.
static bool lookupClientGeometry(const nlohmann::json& data, const std::string& windowID,
                                 WindowPos& pos, pid_t& pid) {
    bool found = false;

    for (const auto& client : data) {
        if (!client.contains("address")) {
            continue;
        }
        if (client["address"].get<std::string>() != windowID) {
            continue;
        }

        pos = windowPosFromClient(client);
        pid = client.contains("pid") ? client["pid"].get<pid_t>() : 0;
        found = true;
    }

    return found;
}

std::string readVideoPath() {
    const char* homeDir = std::getenv("HOME");
    std::string configPath = homeDir ? std::string(homeDir) + "/.config/HyprLarp.json" : "HyprLarp.json";
    simdjson::dom::parser parser;
    auto doc = parser.load(configPath);
    auto path = doc["videoPath"];
    if (path.error()) {
        throw std::runtime_error("videoPath not found in config");
    }
    return std::string(path.value().get_string().value());
}

// Function to get videometadata
int WindowData::GetVideoData() {
    

    const char* filename = this->vidData.videoPath.c_str();
    AVFormatContext* format_ctx = nullptr;
    

    if (avformat_open_input(&format_ctx, filename, nullptr, nullptr) != 0) {
        std::cerr << "Could not open file\n";
        std::cout << filename << std::endl;

        throw std::runtime_error("avformat_open_input : Could not open file");

        return -1;
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        std::cerr << "Could not find stream information\n";
        avformat_close_input(&format_ctx);
        return -1;
    }

    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVCodecParameters* codec_params = format_ctx->streams[i]->codecpar;
            AVStream* stream = format_ctx->streams[i];

            this->vidData.video_w = codec_params->width;
            this->vidData.video_h= codec_params->height;
            
            // Calculate FPS reliably via fractional numbers
            this->vidData.fps = 0.0;

            if (stream->avg_frame_rate.den > 0) {
                this->vidData.fps = static_cast<double>(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;
            }

            break;
        }
    }

    avformat_close_input(&format_ctx);

    return 0;
};

// Function to check wether the config file already exist or not
bool configFileExists(const std::string& path) {
    // std::filesystem::exists returns true if the path exists, false otherwise
    return std::filesystem::exists(path);
}

// Parse ~/.config/HyprLarp.json
videoPos jsonParser() {
    simdjson::dom::parser parser;

    const char* homeDir = std::getenv("HOME");
    std::string filePath = homeDir ? std::string(homeDir) + "/.config/HyprLarp.json" : "HyprLarp.json";
    simdjson::dom::element doc = parser.load(filePath);

    auto corners = doc["cornerLocation"];

    // Mapiing 
    int64_t l = corners.at(0).at(0).get_int64();
    int64_t r = corners.at(3).at(0).get_int64();
    int64_t t = corners.at(0).at(1).get_int64();
    int64_t b = corners.at(3).at(1).get_int64();

    return {
        static_cast<int>(l),
        static_cast<int>(r),
        static_cast<int>(t),
        static_cast<int>(b)
    };
};

// Constructor 
WorkspaceData::WorkspaceData() {
    if (GetWindowsPropertiesData(WorkspaceData::data) != 0) {
        throw std::runtime_error("Failed to get window properties from Hyprland");
    }
    
    // Pupulate the data part
    // GetWindowsPropertiesData(WorkspaceData::data);

    // Change implementation to find main window
    WorkspaceData::setWorkspaceIDStartup();

    // Inttitiae the pos video
    this->vidPos = jsonParser();

    // Put main terminal window ID into windowData type in the array
    //     std::cout << std::endl << this->mainTerminalWindowID << std::endl;

    WorkspaceData::InsertWindowData(this->mainTerminalWindowID, 1);

}

// Constructor function
// updated version
WindowData::WindowData(int windowType, std::string windowID, nlohmann::json& data, videoPos pos) {
    
    // Put requirement data into the object
    this->windowID = windowID;
    this->windowType = windowType;
    this->vidData.videoPath = readVideoPath();


    // this->windowPos = pos; 

    // Find position of the window
    this->windowPos = GetWindowPos(data);
    
    pid = 0;  // default
    if (data.is_array()) {
        for (const auto& client : data) {
            if (client.contains("address") && client["address"].get<std::string>() == this->windowID) {
                if (client.contains("pid")) {
                    pid = client["pid"].get<pid_t>();
                    break;
                }
            }
        }
    }

    // Get internal data
    this->internalTerminalGeometry = GetInternalTerminalGeometry(this->windowPos);

    // Stup InternalWindowData
    this->internalWindowPos = {
        this->internalTerminalGeometry.grid_screen_x,
        this->internalTerminalGeometry.grid_screen_x + this->internalTerminalGeometry.w,
        this->internalTerminalGeometry.grid_screen_y + this->internalTerminalGeometry.h,
        this->internalTerminalGeometry.grid_screen_y
    };

    // Find cartesian of the window
    // queryPosWindow();
    // this->windowPosCartesian = ConvertPosFormat(this->windowPos);
    GetWindowPosCartesian();

    // GetVideoData();
    this->vidData = getCachedVideoData(this->vidData.videoPath);

    // Setup viewport
    // GetOverlap(pos.video_left, pos.video_top, pos.video_right, pos.video_bottom, this->internalTerminalGeometry, this->viewPort);

    // debug / test
    std::cerr << "layoutCalculation inputs:"
          << " v_left=" << pos.video_left << " v_top=" << pos.video_top
          << " v_right=" << pos.video_right << " v_bottom=" << pos.video_bottom
          << " term_grid_x=" << internalTerminalGeometry.grid_screen_x
          << " term_grid_y=" << internalTerminalGeometry.grid_screen_y
          << " term_w=" << internalTerminalGeometry.w
          << " term_h=" << internalTerminalGeometry.h
          << " cell_w=" << internalTerminalGeometry.cell_w
          << " cell_h=" << internalTerminalGeometry.cell_h
          << std::endl;

    // Setup terminalLayout
    layoutCalculation(pos.video_left, pos.video_top, pos.video_right, pos.video_bottom, this->internalTerminalGeometry, this->viewPort, this->windowPos, this->vidData, this->layoutRender);

}

WindowData::WindowData(int windowType, std::string windowID, const WindowPos& pos, pid_t pid, videoPos vpos)
{
    this->windowID = windowID;
    this->windowType = windowType;
    this->windowPos = pos;
    this->pid = pid;
    this->vidData.videoPath = readVideoPath();

    // The rest is identical to your existing constructor:
    this->internalTerminalGeometry = GetInternalTerminalGeometry(this->windowPos);
    this->internalWindowPos = {
        this->internalTerminalGeometry.grid_screen_x,
        this->internalTerminalGeometry.grid_screen_x + this->internalTerminalGeometry.w,
        this->internalTerminalGeometry.grid_screen_y + this->internalTerminalGeometry.h,
        this->internalTerminalGeometry.grid_screen_y
    };
    GetWindowPosCartesian();
    // GetVideoData();
    this->vidData = getCachedVideoData(this->vidData.videoPath);
    layoutCalculation(vpos.video_left, vpos.video_top, vpos.video_right, vpos.video_bottom,
                      this->internalTerminalGeometry, this->viewPort, this->windowPos, this->vidData, this->layoutRender);
}


// Overload Constructor function
// why would i do this??
// deprecated
WindowData::WindowData(int windowType) {
    // Get window data (see improvement part in todo.txt)
    nlohmann::json data;
    GetWindowsPropertiesData(data);

    // Get windowID
    std::string windowID;
    pid_t pid = FindTerminalPID();
    GetWindowAddress(pid, windowID, &data);
    
    // Put requirement data into the object
    this->windowID = windowID;
    this->windowType = windowType;

    // Find position of the window
    this->windowPos = GetWindowPos(data);

    // Get internal data
    this->internalTerminalGeometry = GetInternalTerminalGeometry(this->windowPos);

    // Stup InternalWindowData
    this->internalWindowPos = {
        this->internalTerminalGeometry.grid_screen_x,
        this->internalTerminalGeometry.grid_screen_x + this->internalTerminalGeometry.w,
        this->internalTerminalGeometry.grid_screen_y + this->internalTerminalGeometry.h,
        this->internalTerminalGeometry.grid_screen_y
    };

    // Find cartesian of the window
    // queryPosWindow();
    // this->windowPosCartesian = ConvertPosFormat(this->windowPos);
    GetWindowPosCartesian();


    // Setup viewport
    // GetOverlap(this->, int v_y1, int v_x2, int v_y2, InternalTerminalGeometry internalTerminalGeometry, ViewportState &viewPort)
}

// ts shit
int WindowData::GetWindowPosCartesian() {
    this->windowPosCartesian = ConvertPosFormat(this->windowPos);
    return 1;
}

// Function that input hyprctl client data and output and the current windowID of the object and output the WindowPos object of its pos, size
WindowPos WindowData::GetWindowPos(nlohmann::json& data) {
    std::mutex mutex;
    WindowPos output;
    
    // why i need double vector of int? yeah actually jst delete it
    if (queryPosWindow(data, this->windowID, output) == 1) {
        std::cerr << "Cant find post window somehow";
    };
    
    return output;
}

// Function to return given terminal window tittle name's window address
std::string WorkspaceData::windowID(std::string TerminalWindowTittleName) {

    // Find the main terminal window ID and workspace ID of it
    std::mutex socketSendMutex;
    std::string windowsAddress;

    int socketSend = GetWindowAddress(TerminalWindowTittleName, socketSendMutex, windowsAddress, &this->data);

    if (socketSend !=0) {
        throw std::runtime_error("Failed to found window tittle name");
    }

    return windowsAddress;
}

int WorkspaceData::FindWorkspaceID(std::string& windowID) {
    return queryWorkspaceId(this->data, windowID);
} 

// Function to insert a windowData to the workspaceData from window ID and window Type
// window type {1: main, 0 : sub}
int WorkspaceData::InsertWindowData(std::string& windowID, int windowType) {
    WindowPos pos;
    pid_t windowPid = 0;

    if (!lookupClientGeometry(this->data, windowID, pos, windowPid)) {
        std::cerr << "InsertWindowData: no client entry for " << windowID << std::endl;
        return 0;
    }

    this->windowData.emplace_back(windowType, windowID, pos, windowPid, this->vidPos);
    this->windowData.back().vidData.videoPath = this->videoPath;

    return 1;
}

// Fetch all terminal windowID that have workspaceID
// Output of vector from windowID
int WorkspaceData::FetchWindowID() {
    GetWindowsPropertiesData(this->data);

    std::unordered_set<std::string> currentWindow = WorkspaceData::currentWindowData();

    // Deduplicate by address; keep the last workspace entry for each window.
    std::unordered_map<std::string, nlohmann::json> workspaceClients;
    for (const auto& jsonData : GetAllWindowOfaWorkspaceID(this->data, this->WorkspaceID)) {
        if (!IsPIDTerminal(jsonData["pid"])) {
            continue;
        }
        workspaceClients[jsonData["address"].get<std::string>()] = jsonData;
    }

    for (const auto& [address, jsonData] : workspaceClients) {
        if (currentWindow.count(address)) {
            continue;
        }

        WindowPos pos = windowPosFromClient(jsonData);
        pid_t pid = jsonData["pid"].get<pid_t>();
        this->windowData.emplace_back(0, address, pos, pid, this->vidPos);
    }
    return 1;
};

std::unordered_set<std::string> WorkspaceData::currentWindowData() {
    std::unordered_set<std::string> output;
    
    for (const auto& i: this->windowData) {

        output.insert(i.getWindowID());   
    }

    return output;
};


static void PrintVec(std::ostream& os, const std::vector<int>& vec) {
    os << "[";
    
    for (size_t i = 0; i < vec.size(); i++) {
        os << vec[i];

        if (i != vec.size() - 1)
            os << ", ";
    }

    os << "]";
}

std::ostream& operator<<(std::ostream& os, const WindowData& wd) {
    os << "WindowData {\n";

    os << "  windowID: " << wd.windowID << "\n";
    os << "  windowType: " << wd.windowType << "\n";

    os << "  at: ";
    PrintVec(os, wd.windowPos.at);
    os << "\n";

    os << "  size: ";
    PrintVec(os, wd.windowPos.size);
    os << "\n";

    os << "  topLeft: ";
    PrintVec(os, wd.windowPosCartesian.topLeft);
    os << "\n";

    os << "  topRight: ";
    PrintVec(os, wd.windowPosCartesian.topRight);
    os << "\n";

    os << "  bottomLeft: ";
    PrintVec(os, wd.windowPosCartesian.bottomLeft);
    os << "\n";

    os << "  bottomRight: ";
    PrintVec(os, wd.windowPosCartesian.bottomRight);
    os << "\n";

    os << "}";

    return os;
}

std::ostream& operator<<(std::ostream& os, const WorkspaceData& ws) {
    os << "WorkspaceData {\n";

    os << "  WorkspaceID: " << ws.WorkspaceID << "\n";
    os << "  mainTerminalWindowID: " << ws.mainTerminalWindowID << "\n";

    os << "  windowData:\n";

    for (const auto& window : ws.windowData) {
        os << window << "\n";
    }

    os << "}";

    return os;
}


int WorkspaceData::setWorkspaceIDStartup() {

    // Old implementation
    // std::string TerminalWindowTittleName = ChangeWindowTittleName();

    // // Get main terminal window ID (address)
    // this->mainTerminalWindowID = WorkspaceData::windowID(TerminalWindowTittleName);
    

    // // Set workspaceID to the main terminal window ID
    // this->WorkspaceID = WorkspaceData::FindWorkspaceID(this->mainTerminalWindowID);

    // New implementation
    pid_t pid = FindTerminalPID();
//     std::cout << "terminal pid found is : " << pid << std::endl;
    if (pid != -1) {
        // I neeed to convert it first from PID into thw window ID

        GetWindowAddress(pid, this->mainTerminalWindowID, &this->data);

        this->WorkspaceID = WorkspaceData::FindWorkspaceID(this->mainTerminalWindowID);
        return 1;
    };

    std::runtime_error("cant fine main terminal");
    return 0;

};

