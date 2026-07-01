cmake_minimum_required(VERSION 3.28) # Recent version for better c++26 compatibility
project(CoreEngine VERSION 1.0)

# Requires C++26 at least
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(SDL2 REQUIRED) # Looks into /usr/lib or /usr/local/lib for SDL2
include_directories(${SDL2_INCLUDE_DIRS}) # Injects SDL2 dirs as flags for the compiler find the header files

add_library(core_engine SHARED 
    src/api.cpp
    src/api.hpp
    src/CoreEngine.hpp
    src/CoreEngine.cpp
)

target_compile_options(core_engine PRIVATE -std=c++26 -O3 -Wall -Wextra)

target_link_libraries(core_engine PRIVATE ${SDL2_LIBRARIES}) # links the program and SDL2 bynaries. SDL2_LIBRARIES contains the exact path for the SDL2 .so on the system