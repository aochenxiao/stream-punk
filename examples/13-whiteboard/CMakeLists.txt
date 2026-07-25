cmake_minimum_required(VERSION 3.20)
project(example-13-whiteboard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# C++ 画板服务器
add_executable(whiteboard-server
    server/main.cpp
    server/src/RoomManager.cpp
)
target_include_directories(whiteboard-server PRIVATE
    ${PROJECT_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/server          # server/ (customData.hpp 在此)
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
)
if(WIN32)
    target_link_libraries(whiteboard-server PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(whiteboard-server PRIVATE /utf-8 /Zc:preprocessor /bigobj /FS)
endif()