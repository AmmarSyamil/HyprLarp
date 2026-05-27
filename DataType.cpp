// File of the project data type

#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <list>
// #include 'socket'

#include "DataType.hpp"
#include "socketSend.hpp"
#include "tools.hpp"
#include "layoutWindow.hpp"

#include "checkTerminal.hpp"
#include <unordered_set>


// Constructor 
WorkspaceData::WorkspaceData() {
    // Pupulate the data part
    GetWindowsPropertiesData(WorkspaceData::data);

    // Change implementation to find main window

    WorkspaceData::setWorkspaceIDStartup();

    // Put main terminal window ID into windowData type in the array
    std::cout << std::endl << this->mainTerminalWindowID << std::endl;
    WorkspaceData::InsertWindowData(this->mainTerminalWindowID, 1);
}

// Constructor function
WindowData::WindowData(int windowType, std::string windowID, nlohmann::json& data) {
    
    // Put requirement data into the object
    this->windowID = windowID;
    this->windowType = windowType;
    
    // Find position of the window
    this->windowPos = GetWindowPos(data);

    // Find cartesian of the window
    // queryPosWindow();
    // this->windowPosCartesian = ConvertPosFormat(this->windowPos);
    GetWindowPosCartesian();
    
}

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
    this->windowData.emplace_back(WindowData(windowType, windowID, this->data));
    std::cout << "test" << std::endl;
    return 1;
}

// Fetch all terminal windowID that have workspaceID
// Output of vector from windowID
int WorkspaceData::FetchWindowID() {
    std::unordered_set<std::string> currentWindow = WorkspaceData::currentWindowData();

    for (const auto& jsonData: GetAllWindowOfaWorkspaceID(this->data, this->WorkspaceID)) {
        if (IsPIDTerminal(jsonData["pid"]) == 1 and !currentWindow.count(jsonData["address"])) {
            // I think i need to change the normal json data into the window data format.
            this->windowData.emplace_back(0, jsonData["address"], this->data);
        }
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
    std::cout << "terminal pid found is : " << pid << std::endl;
    if (pid != -1) {
        // I neeed to convert it first from PID into thw window ID

        GetWindowAddress(pid, this->mainTerminalWindowID, &this->data);

        this->WorkspaceID = WorkspaceData::FindWorkspaceID(this->mainTerminalWindowID);
        return 1;
    };

    std::runtime_error("cant fine main terminal");
    return 0;

};

