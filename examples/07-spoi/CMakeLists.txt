cmake_minimum_required(VERSION 3.20)
project(example-07-spoi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(example-07-spoi main.cpp)

target_include_directories(example-07-spoi PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(example-07-spoi PRIVATE doctest::doctest)

# MSVC 需要 /utf-8 和 /Zc:preprocessor；/bigobj 因模板展开过多
if(MSVC)
    target_compile_options(example-07-spoi PRIVATE /utf-8 /Zc:preprocessor /bigobj)
endif()