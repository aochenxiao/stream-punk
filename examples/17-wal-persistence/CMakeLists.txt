cmake_minimum_required(VERSION 3.20)
project(example-17-wal-persistence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-17-wal-persistence main.cpp)

target_include_directories(example-17-wal-persistence PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-17-wal-persistence PRIVATE doctest::doctest)