cmake_minimum_required(VERSION 3.20)
project(MetaExtractor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
add_executable(meta_extractor meta_extractor.cpp)
target_include_directories(meta_extractor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
if(MSVC)
    target_link_options(meta_extractor PRIVATE /FORCE:UNRESOLVED)
endif()