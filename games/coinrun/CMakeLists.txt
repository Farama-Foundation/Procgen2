cmake_minimum_required(VERSION 3.13)

include(FetchContent)

project(CoinRun)

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
# Get Raylib

set(PLATFORM "Desktop") # Set to DRM for headless

FetchContent_Declare(
    Raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib
    GIT_TAG master
)

FetchContent_GetProperties(Raylib)

if(NOT Raylib_POPULATED)
    FetchContent_Populate(Raylib)
    add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
    set_target_properties(raylib PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

############################################################################

include_directories(".")

include_directories("${PROJECT_SOURCE_DIR}")

set(SOURCE_PATH "${PROJECT_SOURCE_DIR}")
set(SOURCES
    "${SOURCE_PATH}/coinrun.cpp"
)

add_library(CoinRun SHARED ${SOURCES})

target_link_libraries(CoinRun PRIVATE raylib)

set_target_properties(CoinRun PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

