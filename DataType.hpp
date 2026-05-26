#pragma once

#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>

struct WindowPos {
    std::vector<int> at;
    std::vector<int> size;
};


// Cartesian position in [x, y]
struct WindowPosCartesian {
    std::vector<int> topLeft; // top left point
    std::vector<int> topRight; // top right point
    std::vector<int> bottomLeft; // bottom left point
    std::vector<int> bottomRight; // bottom right point
};

class WindowData {
private:

    //Window pos in [at, size]
    WindowPos windowPos;

    WindowPosCartesian windowPosCartesian;

    std::string windowID;

    // window type {1: main, 0 : sub}
    int windowType;

    public:

    std::vector<int> windowPosition;
    
    WindowPos GetWindowPos(nlohmann::json& data);

    // Function to populate cartesian from windowPos
    int GetWindowPosCartesian();

    // Constructor
    WindowData(int windowType, std::string windowID, nlohmann::json& data);

    friend std::ostream& operator<<(std::ostream& os, const WindowData& wd);
};

class WorkspaceData {

private:
    // Hyprctl client -j full data
    nlohmann::json data;

    // Vector containing all data of the window properties
    std::vector<WindowData> windowData;
    
    //fetch from initial
    int WorkspaceID;

public:
    //Main terminal window address
    std::string mainTerminalWindowID;

    // Function to find workspace ID (address) from given terminal window tittle name
    std::string windowID(std::string TerminalWindowTittleName);

    // Function to find workspace ID from given window ID (address)
    int FindWorkspaceID(std::string& windowID);

    // Put window ID into the windowdata
    int InsertWindowData(std::string& windowID, int windowType);

    // Fetch all windowID that have workspaceID and put it into windowData
    int FetchWindowID();

    // refresh data
    // this shit propably going to be hard asf, I need to fetch from socketRecieve of the new updated data.

    // Constructor
    WorkspaceData();

    friend std::ostream& operator<<(std::ostream& os, const WorkspaceData& ws);
};
