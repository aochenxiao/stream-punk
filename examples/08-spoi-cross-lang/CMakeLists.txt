cmake_minimum_required(VERSION 3.20)
project(example-08-spoi-cross-lang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++ 服务器
add_executable(spoi-cross-server server/main.cpp)

target_include_directories(spoi-cross-server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(spoi-cross-server PRIVATE doctest::doctest)

# 链接 Winsock2
if(WIN32)
    target_link_libraries(spoi-cross-server PRIVATE ws2_32)
endif()

# MSVC 需要 /utf-8 和 /Zc:preprocessor
if(MSVC)
    target_compile_options(spoi-cross-server PRIVATE /utf-8 /Zc:preprocessor)
endif()