cmake_minimum_required(VERSION 3.20)
project(TankBattleServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tank-battle-server server.cpp)
target_include_directories(tank-battle-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(tank-battle-server PRIVATE ws2_32)

if(MSVC)
    target_compile_options(tank-battle-server PRIVATE /utf-8 /Zc:preprocessor /bigobj /FS)
    target_link_options(tank-battle-server PRIVATE /FORCE:UNRESOLVED)
endif()