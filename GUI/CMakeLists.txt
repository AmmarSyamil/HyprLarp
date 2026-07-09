cmake_minimum_required(VERSION 3.16)
project(HyprLarp)

# ---------- Qt ----------
find_package(Qt6 COMPONENTS Widgets REQUIRED)   # or Qt5 if you use that

# ---------- OpenCV ----------
find_package(OpenCV REQUIRED)

# ---------- Executable ----------
add_executable(GUI
    ../gui.cpp
    # add other source files here
)

# ---------- Link libraries ----------
target_link_libraries(GUI
    Qt${QT_VERSION_MAJOR}::Widgets
    ${OpenCV_LIBS}          # links the OpenCV libraries and adds include directories automatically
)

# ---------- Optional Qt automoc ----------
set_target_properties(GUI PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# ---------- Compile options ----------
if(UNIX)
    target_compile_options(GUI PRIVATE -Wall -Wextra -Wpedantic)
endif()