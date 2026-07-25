cmake_minimum_required(VERSION 3.20)
project(example-02-cpp-to-ts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-02-cpp-to-ts main.cpp)

target_include_directories(example-02-cpp-to-ts PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-02-cpp-to-ts PRIVATE doctest::doctest)