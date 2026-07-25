cmake_minimum_required(VERSION 3.20)
project(example-06-shadow-delta LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-06-shadow-delta main.cpp)

target_include_directories(example-06-shadow-delta PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-06-shadow-delta PRIVATE doctest::doctest)