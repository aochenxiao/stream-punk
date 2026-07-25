cmake_minimum_required(VERSION 3.16)
project(WebSocketEchoServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Drogon包
find_package(Drogon REQUIRED)

# 添加可执行文件
add_executable(${PROJECT_NAME} test.cpp)

# 链接Drogon库
target_link_libraries(${PROJECT_NAME} Drogon::Drogon)

# 如果你需要额外的库，可以在这里添加
# target_link_libraries(${PROJECT_NAME} pthread)