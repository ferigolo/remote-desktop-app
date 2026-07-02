cmake_minimum_required(VERSION 3.28) # Recent version for better c++26 compatibility
project(CoreEngine VERSION 1.0)

# Requires C++26 at least
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# We need to tell CMake to look /usr/local
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake" "/usr/local/lib/cmake/SDL3")

find_package(SDL3 REQUIRED) # Looks into /usr/lib or /usr/local/lib for SDL3
include_directories(${SDL3_INCLUDE_DIRS}) # Injects SDL3 dirs as flags so the compiler can find the header files

set(SOURCE_FILES 
    src/main.cpp 
    src/CoreEngine.mm
    src/configurers/MediaEngineConfigurer.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "🐧 Linux system detected: Activating DBus and PipeWire support")
    # Add the Linux capturer into SOURCE_FILES to compile
    list(APPEND SOURCE_FILES src/capturers/LinuxCapturer.cpp) 
    
    # Force Linux compiler to treat .mm as standard C++ to avoid requiring gobjc++
    set_source_files_properties(src/CoreEngine.mm PROPERTIES LANGUAGE CXX)

    # Find needed packages to run
    # pkg-config -> terminal utility which knows exactly where apt installed each package
    find_package(PkgConfig REQUIRED) # Checks if the pkg-config is installed in the system
    # Because pkg-config is installed, we use it to find DBus and PipeWire (they are not part of the CMake modules)
    pkg_check_modules(DBUS REQUIRED dbus-1) 
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

    # Where the headers of DBus and PipeWire are. So when we write '#include <pipewire/pipewire.h>' it does not crash
    include_directories(${DBUS_INCLUDE_DIRS} ${PIPEWIRE_INCLUDE_DIRS})

    # Tells linker where are the binaries (.so) of these packages
    link_directories(${DBUS_LIBRARY_DIRS} ${PIPEWIRE_LIBRARY_DIRS})

    # Macro to C++ engine know we are running on Linux
    add_compile_definitions(__linux__)
    
elseif(APPLE)
    message(STATUS "🍎 MacOS system detected: Preparing ScreenCaptureKit support")
    
    find_library(SCREENCAPTUREKIT ScreenCaptureKit)
    find_library(COREMEDIA CoreMedia)
    find_library(FOUNDATION Foundation)
    
    target_link_libraries(core-engine PRIVATE 
        ${SCREENCAPTUREKIT} 
        ${COREMEDIA} 
        ${FOUNDATION}
    )
endif()

add_executable(core-engine ${SOURCE_FILES})

target_compile_options(core-engine PRIVATE -std=c++26 -O3 -Wall -Wextra)

target_link_libraries(core-engine PRIVATE SDL3::SDL3)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(core-engine PRIVATE ${DBUS_LIBRARIES} ${PIPEWIRE_LIBRARIES})
endif()

# Required by Rust's cmake crate to output the binary correctly
install(TARGETS core-engine DESTINATION bin)