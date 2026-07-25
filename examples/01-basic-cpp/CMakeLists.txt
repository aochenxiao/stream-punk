cmake_minimum_required(VERSION 3.20)
project(example-01-basic-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-01-basic-cpp main.cpp)

target_include_directories(example-01-basic-cpp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-01-basic-cpp PRIVATE doctest::doctest)