add_executable(consumer
    ../consumer.cpp
    ../renderer.cpp
    ../shm.cpp
    ../base64converter.cpp
)

target_include_directories(consumer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(consumer PRIVATE stdc++)

# Compile options
if(UNIX)
    target_compile_options(consumer PRIVATE -Wall -Wextra -Wpedantic -g)
endif()
