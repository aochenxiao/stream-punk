cmake_minimum_required(VERSION 3.20)
project(example-05-orm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-05-orm main.cpp)

target_include_directories(example-05-orm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-05-orm PRIVATE doctest::doctest)