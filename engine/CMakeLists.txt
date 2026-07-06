cmake_minimum_required(VERSION 3.30) # Recent version for better c++26 compatibility
project(CoreEngine VERSION 1.0)

# Requires C++26 at least
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# We need to tell CMake to look /usr/local
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake" "/usr/local/lib/cmake/SDL3")

find_package(SDL3 REQUIRED) # Looks into /usr/lib or /usr/local/lib for SDL3
include_directories(${SDL3_INCLUDE_DIRS}) # Injects SDL3 dirs as flags so the compiler can find the header files

set(SOURCE_FILES 
    src/main.cpp 
    src/CoreEngine.cpp
    src/configurers/CoreEngineConfigurer.cpp
    src/network/WebRtcManager.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "🐧 Linux system detected: Activating DBus and PipeWire support")
    # Add the Linux capturer and platform utils into SOURCE_FILES to compile
    list(APPEND SOURCE_FILES 
        src/capturers/linux/PipeWireClient.cpp
        src/capturers/linux/ScreencastPortal.cpp
        src/utils/linux/PlatformUtils.cpp
        src/capturers/LinuxCapturer.cpp
        src/encoders/H264Encoder.cpp
    )

    find_package(Qt6 COMPONENTS Core DBus REQUIRED)
    # Find needed packages to run
    # pkg-config -> terminal utility which knows exactly where apt installed each package
    find_package(PkgConfig REQUIRED) # Checks if the pkg-config is installed in the system

    # Because pkg-config is installed, we use it to find DBus and PipeWire (they are not part of the CMake modules)
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavutil libavfilter libswscale)

    include_directories(${PIPEWIRE_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS} "/usr/local/cuda/include") # Where the headers PipeWire and FFmpeg are. So when we write '#include <pipewire/pipewire.h>' it does not crash
    link_directories(${PIPEWIRE_LIBRARY_DIRS} ${FFMPEG_LIBRARY_DIRS} "/usr/local/cuda/lib64") # Tells linker where are the binaries (.so) of these packages
    
    # Macro to C++ engine know we are running on Linux
    add_compile_definitions(__linux__)
    
elseif(APPLE)
    message(STATUS "🍎 MacOS system detected: Preparing ScreenCaptureKit support")
    
    list(APPEND SOURCE_FILES src/utils/mac/PlatformUtils.mm)

    find_library(SCREENCAPTUREKIT ScreenCaptureKit)
    find_library(COREMEDIA CoreMedia)
    find_library(FOUNDATION Foundation)
    
    target_link_libraries(core-engine PRIVATE 
        ${SCREENCAPTUREKIT} 
        ${COREMEDIA} 
        ${FOUNDATION}
    )
endif()
                 
include(FetchContent)
# We only want the core library, no extra bloat
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE) # Keep WebSockets ON for signaling
# Allow compiling older dependencies (fixes the plog CMake error)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)

# Fetch and build lidatachannel (WebRTC)
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG        v0.21.2
)
FetchContent_MakeAvailable(libdatachannel)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

add_executable(core-engine ${SOURCE_FILES})
target_compile_options(core-engine PRIVATE -std=c++26 -O3 -Wall -Wextra)
target_link_libraries(core-engine PRIVATE SDL3::SDL3 datachannel nlohmann_json::nlohmann_json)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(core-engine PRIVATE ${PIPEWIRE_LIBRARIES} ${FFMPEG_LIBRARIES} Qt6::Core Qt6::DBus)
endif()

# Required by Rust's cmake crate to output the binary correctly
install(TARGETS core-engine DESTINATION bin)

# --- NATIVE CLIENT ---
set(CLIENT_SOURCE_FILES
    src/client/main.cpp
    src/client/ClientApp.cpp
    src/client/ClientWebRtcManager.cpp
    src/client/H264Decoder.cpp
)

add_executable(native-client ${CLIENT_SOURCE_FILES})
target_compile_options(native-client PRIVATE -std=c++26 -O3 -Wall -Wextra)
target_link_libraries(native-client PRIVATE SDL3::SDL3 datachannel nlohmann_json::nlohmann_json)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(native-client PRIVATE ${FFMPEG_LIBRARIES})
endif()