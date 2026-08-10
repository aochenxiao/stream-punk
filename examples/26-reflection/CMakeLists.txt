# stream-punk_for_cpp26 — 示例
# 编译: 需要 GCC 16+ + -std=c++26 -freflection

cmake_minimum_required(VERSION 3.20)

add_executable(sp26-example
    main.cpp
)
target_include_directories(sp26-example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/examples
)
target_compile_features(sp26-example PRIVATE cxx_std_26)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sp26-example PRIVATE -freflection)
endif()