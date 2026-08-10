cmake_minimum_required(VERSION 3.20)
project(example-18-iot-sensor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-18-iot-sensor main.cpp)

target_include_directories(example-18-iot-sensor PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-18-iot-sensor PRIVATE doctest::doctest)