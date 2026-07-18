cmake_minimum_required(VERSION 3.16)
project(HyprLarp)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

find_package(OpenCV REQUIRED)
find_package(simdjson REQUIRED)
add_executable(hyprlarp
    ../main.cpp
    ../socketSend.cpp
    ../socketReceive.cpp
    ../layoutWindow.cpp
    ../tools.cpp
    ../DataType.cpp
    ../checkTerminal.cpp
    ../terminal.cpp
    ../terminal.hpp
    ../checkTerminal.cpp
    ../checkTerminal.hpp
    ../producer.cpp
    ../videoDecoder.cpp
    ../shm.cpp
    ../gui.cpp
    ../consumer.cpp
    ../renderer.cpp
    ../base64converter.cpp
    ../terminalLayout.cpp
    ../consumerLayout.cpp
    ../consumerRegistry.cpp
    ../hyprlandIPC.cpp
)


target_link_libraries(
    hyprlarp PRIVATE
    nlohmann_json::nlohmann_json
    PkgConfig::SH4LT
    Qt${QT_VERSION_MAJOR}::Widgets
    ${OpenCV_LIBS}
    avformat
    avcodec
    avutil
    swscale
    simdjson::simdjson
)

set_target_properties(hyprlarp PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Compile options
if(UNIX)
    target_compile_options(hyprlarp PRIVATE -Wall -Wextra -Wpedantic)
endif()
