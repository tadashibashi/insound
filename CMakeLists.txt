cmake_minimum_required(VERSION 3.28)
project(insound)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(insound)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    add_subdirectory(sandbox)
endif()
