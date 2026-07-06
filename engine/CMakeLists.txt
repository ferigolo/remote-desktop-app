cmake_minimum_required(VERSION 3.30) # Recent version for better c++26 compatibility
project(CoreEngine VERSION 1.0)

# Requires C++26 at least
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_TOOLCHAIN_FILE "/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()
include(FetchContent)

# Fetch and Compile SDL3
set(SDL_TEST OFF CACHE BOOL "" FORCE) # Disable tests to save time
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12
)

# Fetch and Compile libdatachannel (WebRTC)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE) # Keep WebSockets for signaling
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG        v0.24.5
)

# Fetch nlohmann_json (Direct compressed download for speed)
FetchContent_Declare(
    json 
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)

# Make the cloned dependencies available in the project
FetchContent_MakeAvailable(SDL3 libdatachannel json)

# --- SOURCE FILES ---
set(SOURCE_FILES 
    src/main.cpp 
    src/CoreEngine.cpp
    src/configurers/CoreEngineConfigurer.cpp
    src/network/WebRtcManager.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "🐧 Linux system detected: Activating DBus and PipeWire support")
    list(APPEND SOURCE_FILES 
        src/capturers/linux/PipeWireClient.cpp
        src/capturers/linux/ScreencastPortal.cpp
        src/utils/linux/PlatformUtils.cpp
        src/capturers/LinuxCapturer.cpp
        src/encoders/H264Encoder.cpp
    )

    # Find the Qt6 package installed on the system via apt
    find_package(Qt6 COMPONENTS Core DBus REQUIRED)
    
    # Find FFmpeg and PipeWire installed on the system via apt
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavutil libavfilter libswscale)

    include_directories(${PIPEWIRE_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS} "/usr/local/cuda/include")
    link_directories(${PIPEWIRE_LIBRARY_DIRS} ${FFMPEG_LIBRARY_DIRS} "/usr/local/cuda/lib64")
    
    add_compile_definitions(__linux__)
    
elseif(APPLE)
    message(STATUS "🍎 MacOS system detected: Preparing ScreenCaptureKit support")
    list(APPEND SOURCE_FILES src/utils/mac/PlatformUtils.mm)

    find_library(SCREENCAPTUREKIT ScreenCaptureKit)
    find_library(COREMEDIA CoreMedia)
    find_library(FOUNDATION Foundation)
endif()
                 
# --- EXECUTABLE: CORE ENGINE ---
add_executable(core-engine ${SOURCE_FILES})
target_compile_options(core-engine PRIVATE -std=c++26 -O3 -Wall -Wextra)

# Link libraries downloaded via Git (FetchContent)
target_link_libraries(core-engine PRIVATE 
    SDL3::SDL3 
    datachannel 
    nlohmann_json::nlohmann_json
)

# Link system libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(core-engine PROPERTIES AUTOMOC ON)
    target_link_libraries(core-engine PRIVATE 
        ${PIPEWIRE_LIBRARIES} 
        ${FFMPEG_LIBRARIES} 
        Qt6::Core 
        Qt6::DBus
    )
elseif(APPLE)
    target_link_libraries(core-engine PRIVATE 
        ${SCREENCAPTUREKIT} 
        ${COREMEDIA} 
        ${FOUNDATION}
    )
endif()

# Required by Rust's cmake crate to output the binary correctly
install(TARGETS core-engine DESTINATION bin)

# --- EXECUTABLE: NATIVE CLIENT ---
set(CLIENT_SOURCE_FILES
    src/client/main.cpp
    src/client/ClientApp.cpp
    src/client/ClientWebRtcManager.cpp
    src/client/H264Decoder.cpp
)

add_executable(native-client ${CLIENT_SOURCE_FILES})
target_compile_options(native-client PRIVATE -std=c++26 -O3 -Wall -Wextra)

target_link_libraries(native-client PRIVATE 
    SDL3::SDL3 
    datachannel 
    nlohmann_json::nlohmann_json
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(native-client PRIVATE ${FFMPEG_LIBRARIES})
endif()