add_executable(producer 
    ../producer.cpp
    ../videoDecoder.cpp
    ../shm.cpp
)

target_include_directories(producer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(producer PRIVATE stdc++ pthread avformat avcodec avutil swscale)

# Compile options
if(UNIX)
    target_compile_options(producer PRIVATE -Wall -Wextra -Wpedantic -g)
endif()
