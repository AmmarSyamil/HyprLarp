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

// Constructor 
WorkspaceData::WorkspaceData() {
    // Pupulate the data part
    GetWindowsPropertiesData(WorkspaceData::data);

    // Get new terminal window tittle name
    std::string TerminalWindowTittleName = ChangeWindowTittleName();

    // Get main terminal window ID (address)
    WorkspaceData::mainTerminalWindowID = WorkspaceData::windowID(TerminalWindowTittleName);
    
    // Set workspaceID to the main terminal window ID
    this->WorkspaceID = WorkspaceData::FindWorkspaceID(this->mainTerminalWindowID);

    // Put main terminal window ID into windowData type in the array


}

// Constructur
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
    
    // why i need double vector of int? yeah actually
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

    int socketSend = GetWindowAddress(TerminalWindowTittleName, socketSendMutex, windowsAddress);

    if (socketSend==0) {
        throw std::runtime_error("Failed to found window tittle name");
    }

    return windowsAddress;
}

int WorkspaceData::FindWorkspaceID(std::string& windowID) {
    return queryWorkspaceId(this->data, windowID);
} 


int WorkspaceData::InsertWindowData(std::string& windowID, int windowType) {
    WindowData(windowType, windowID, this->data);
    return 1;
}