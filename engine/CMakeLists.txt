cmake_minimum_required(VERSION 3.28) # Recent version for better c++26 compatibility
project(CoreEngine VERSION 1.0)

# Requires C++26 at least
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(core_engine SHARED 
    src/api.cpp 
    src/CoreEngine.hpp
)

target_compile_options(core_engine PRIVATE -O3 -Wall -Wextra)