cmake_minimum_required(VERSION 3.10)
project(HyprLarp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(nlohmann_json 3.2.0 QUIET)

# If nlohmann_json is not found, try to fetch it
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, attempting to fetch from GitHub...")
    include(FetchContent)
    FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
    FetchContent_MakeAvailable(json)
endif()

# Main executable
add_executable(hyprlarp
    main.cpp
    socketSend.cpp
    socketReceive.cpp
    dataChecker.cpp
    layoutWindow.cpp
    tools.cpp
)

# Link libraries
target_link_libraries(hyprlarp PRIVATE nlohmann_json::nlohmann_json)

# Include directories if needed
target_include_directories(hyprlarp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Optional: Add compile options for better warnings
if(UNIX)
    target_compile_options(hyprlarp PRIVATE -Wall -Wextra -Wpedantic)
endif()
