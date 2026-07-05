add_executable(hyprlarp
    ../main.cpp
    ../socketSend.cpp
    ../socketReceive.cpp
    ../dataChecker.cpp
    ../layoutWindow.cpp
    ../tools.cpp
    ../DataType.cpp
    ../checkTerminal.cpp
    ../videoRendering.cpp
    ../terminal.cpp
    ../terminal.hpp
    ../checkTerminal.cpp
    ../checkTerminal.hpp
)

target_include_directories(hyprlarp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(hyprlarp PRIVATE nlohmann_json::nlohmann_json PkgConfig::SH4LT)

# Compile options
if(UNIX)
    target_compile_options(hyprlarp PRIVATE -Wall -Wextra -Wpedantic)
endif()
