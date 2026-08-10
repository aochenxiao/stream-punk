cmake_minimum_required(VERSION 3.20)
project(example-20-microservice-mesh LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-20-microservice-mesh main.cpp)

target_include_directories(example-20-microservice-mesh PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-20-microservice-mesh PRIVATE doctest::doctest)