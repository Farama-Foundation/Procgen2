cmake_minimum_required(VERSION 3.13)

project(Maze)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE OFF)

if(NOT CMAKE_BUILD_TYPE)
    message("CMAKE_BUILD_TYPE not set, setting it to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

############################################################################
# Get SDL

find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

############################################################################

include_directories(".")

include_directories("${PROJECT_SOURCE_DIR}")

set(SOURCE_PATH "${PROJECT_SOURCE_DIR}")
set(SOURCES
    "${SOURCE_PATH}/maze.cpp"
    "${SOURCE_PATH}/ecs.cpp"
    "${SOURCE_PATH}/helpers.cpp"
    "${SOURCE_PATH}/renderer.cpp"
    "${SOURCE_PATH}/common_assets.cpp"
    "${SOURCE_PATH}/common_systems.cpp"
    "${SOURCE_PATH}/maze_generator.cpp"
    "${SOURCE_PATH}/tilemap.cpp"
)

add_library(Maze SHARED ${SOURCES})

target_link_libraries(Maze SDL3::SDL3 SDL3_image)

set_target_properties(Maze PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

