cmake_minimum_required(VERSION 3.20)
project(example-19-leaderboard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-19-leaderboard main.cpp)

target_include_directories(example-19-leaderboard PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-19-leaderboard PRIVATE doctest::doctest)