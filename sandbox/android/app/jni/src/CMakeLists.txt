cmake_minimum_required(VERSION 3.6)

project(main)

add_library(${PROJECT_NAME} SHARED android_main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main sandbox_common log android)
