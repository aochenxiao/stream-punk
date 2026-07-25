cmake_minimum_required(VERSION 3.20)
project(example-10-game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set(DOCTEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3/x64-windows/include)

# C++ 游戏服务器
add_executable(stream-worms-server
    server/main.cpp
    server/src/GameLogic.cpp
    server/src/MessageHandler.cpp
)
target_include_directories(stream-worms-server PRIVATE
    ${PROJECT_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
)
if(WIN32)
    target_link_libraries(stream-worms-server PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(stream-worms-server PRIVATE /utf-8 /Zc:preprocessor)
endif()

# 单元测试
enable_testing()
add_executable(stream-worms-tests
    tests/test_main.cpp
    tests/test_terrain.cpp
    tests/test_movement.cpp
    tests/test_physics.cpp
    server/src/GameLogic.cpp
)
target_include_directories(stream-worms-tests PRIVATE
    ${PROJECT_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${DOCTEST_INCLUDE_DIR}
)
if(WIN32)
    target_link_libraries(stream-worms-tests PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(stream-worms-tests PRIVATE /utf-8 /Zc:preprocessor)
endif()

add_test(NAME stream-worms-tests COMMAND stream-worms-tests)
