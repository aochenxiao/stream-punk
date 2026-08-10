cmake_minimum_required(VERSION 3.20)
project(CollabEditorServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ixwebsocket CONFIG REQUIRED)

add_executable(collab-server server.cpp)
target_include_directories(collab-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(collab-server PRIVATE ixwebsocket::ixwebsocket)

if(MSVC)
    target_compile_options(collab-server PRIVATE /utf-8 /Zc:preprocessor /bigobj /FS)
    target_link_options(collab-server PRIVATE /FORCE:UNRESOLVED)
    target_link_libraries(collab-server PRIVATE bcrypt)
endif()