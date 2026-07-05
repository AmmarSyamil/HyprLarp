#pragma once

#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <unordered_set>

struct WindowPos {
    std::vector<int> at;
    std::vector<int> size;
};


// Cartesian position in [x, y]
// Yeah i think i dont need this cuz i need at and size instead lmao
struct WindowPosCartesian {
    std::vector<int> topLeft; // top left point
    std::vector<int> topRight; // top right point
    std::vector<int> bottomLeft; // bottom left point
    std::vector<int> bottomRight; // bottom right point
};

struct InternalTerminalGeometry {
    int w, h;                 // Pixel size of the text grid (inner area, excludes title bar/borders)
    int cols, rows;           // Number of character cells in the grid (e.g., 80x24)
    int pad_x, pad_y;         // Pixel offset from outer window to the text grid (title bar + left/right padding)
    int grid_screen_x, grid_screen_y; // Absolute screen pixel coordinates of the grid's top-left corner
    int cell_w, cell_h;       // Pixel size of one character cell (text_w / cols, text_h / rows)
};

class WindowData {
private:

    //Window pos in [at, size]
    WindowPos windowPos;

    // Window pos in cartesian point 4 of em.
    // maybe deprecated later
    WindowPosCartesian windowPosCartesian;

    // Internal terminal geometry dimention
    InternalTerminalGeometry internalTerminalGeometry;

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
    WindowData(int windowType);

    friend std::ostream& operator<<(std::ostream& os, const WindowData& wd);

    const std::string& getWindowID() const {
        return windowID;
    }
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

    // function that check terminal of where the program is run and find its workspaceID
    int setWorkspaceIDStartup();

    // Output all of the window ID that currently in the object windowData
    std::unordered_set<std::string> currentWindowData();

    // refresh data
    // this shit propably going to be hard asf, I need to fetch from socketRecieve of the new updated data.

    // Constructor
    WorkspaceData();

    friend std::ostream& operator<<(std::ostream& os, const WorkspaceData& ws);
};
