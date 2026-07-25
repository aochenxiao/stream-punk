cmake_minimum_required(VERSION 3.20)
project(example-09-spoi-cross-lang-all LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 项目根目录 include 路径
set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# C++ 服务器
add_executable(spoi-cross-server-09 server/main.cpp)
target_include_directories(spoi-cross-server-09 PRIVATE
    ${PROJECT_INCLUDE_DIR}
)
target_link_libraries(spoi-cross-server-09 PRIVATE doctest::doctest)
if(WIN32)
    target_link_libraries(spoi-cross-server-09 PRIVATE ws2_32)
endif()

# C++ 客户端
add_executable(spoi-cross-client-cpp cpp/client.cpp)
target_include_directories(spoi-cross-client-cpp PRIVATE
    ${PROJECT_INCLUDE_DIR}
)
target_link_libraries(spoi-cross-client-cpp PRIVATE doctest::doctest)
if(WIN32)
    target_link_libraries(spoi-cross-client-cpp PRIVATE ws2_32)
endif()

# MSVC 需要 /utf-8 和 /Zc:preprocessor（StreamPunk 使用 X-macro 需要符合标准的预处理器）
if(MSVC)
    target_compile_options(spoi-cross-server-09 PRIVATE /utf-8 /Zc:preprocessor)
    target_compile_options(spoi-cross-client-cpp PRIVATE /utf-8 /Zc:preprocessor)
endif()