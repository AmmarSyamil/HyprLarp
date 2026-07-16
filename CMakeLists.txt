cmake_minimum_required(VERSION 3.10)
project(HyprLarp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(nlohmann_json 3.2.0 QUIET)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SH4LT REQUIRED IMPORTED_TARGET sh4lt-0.3)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, attempting to fetch from GitHub...")
    include(FetchContent)
    FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
    FetchContent_MakeAvailable(json)
endif()

add_subdirectory(main)
add_subdirectory(producer) # Build in /HyprLarp/build/producer
add_subdirectory(consumer)
add_subdirectory(GUI)