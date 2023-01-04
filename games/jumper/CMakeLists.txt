cmake_minimum_required(VERSION 3.13)

project(Jumper)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

if(NOT CMAKE_BUILD_TYPE)
    message("CMAKE_BUILD_TYPE not set, setting it to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

############################################################################
# Get SDL2

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

############################################################################

include_directories(".")

include_directories("${PROJECT_SOURCE_DIR}")

set(SOURCE_PATH "${PROJECT_SOURCE_DIR}")
set(SOURCES
    "${SOURCE_PATH}/jumper.cpp"
    "${SOURCE_PATH}/ecs.cpp"
    "${SOURCE_PATH}/helpers.cpp"
    "${SOURCE_PATH}/renderer.cpp"
    "${SOURCE_PATH}/common_assets.cpp"
    "${SOURCE_PATH}/common_systems.cpp"
    "${SOURCE_PATH}/maze_generator.cpp"
    "${SOURCE_PATH}/room_generator.cpp"
    "${SOURCE_PATH}/tilemap.cpp"
)

add_library(Jumper SHARED ${SOURCES})

target_link_libraries(Jumper SDL2::Main SDL2::Image)

set_target_properties(Jumper PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

