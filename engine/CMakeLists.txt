cmake_minimum_required(VERSION 3.30) # Recent version for better c++26 compatibility
project(CoreEngine VERSION 1.0)

# Requires C++26 at least
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()
include(FetchContent)
find_package(PkgConfig REQUIRED)

# Fetch and Compile SDL3
set(SDL_TEST OFF CACHE BOOL "" FORCE) # Disable tests to save time
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12
    FIND_PACKAGE_ARGS NAMES SDL3 # First tries to find an existing SDL3
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
    FIND_PACKAGE_ARGS NAMES LibDataChannel
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
    src/encoders/BaseEncoder.cpp
    src/encoders/software/SoftwareEncoder.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "🐧 Linux system detected: Activating DBus and PipeWire support")
    list(APPEND SOURCE_FILES 
        src/capturers/linux/PipeWireClient.cpp
        src/capturers/linux/ScreencastPortal.cpp
        src/utils/linux/PlatformUtils.cpp
        src/capturers/LinuxCapturer.cpp
    )

    # Find the Qt6 package installed on the system via apt
    find_package(Qt6 COMPONENTS Core DBus REQUIRED)
    
    # Find FFmpeg and PipeWire installed on the system via apt
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavutil libavfilter libswscale)

    include_directories(${PIPEWIRE_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS})
    link_directories(${PIPEWIRE_LIBRARY_DIRS} ${FFMPEG_LIBRARY_DIRS})

    add_compile_definitions(__linux__)
    
elseif(APPLE)
    message(STATUS "🍎 MacOS system detected: Preparing ScreenCaptureKit support")
    list(APPEND SOURCE_FILES src/utils/mac/PlatformUtils.mm)

    find_library(SCREENCAPTUREKIT ScreenCaptureKit)
    find_library(COREMEDIA CoreMedia)
    find_library(FOUNDATION Foundation)
endif()

# --- HARDWARE ACCELERATION DETECTION ---
find_package(CUDAToolkit QUIET) # Check for the CUDA SDK/Toolkit
pkg_check_modules(LIBVA QUIET libva libva-drm libmfx) # Intel QSV / VAAPI Detection

if(CUDAToolkit_FOUND)
    message(STATUS "🟢 CUDA Toolkit found: Enabling CUDA support...")
    list(APPEND SOURCE_FILES src/encoders/nvidia/CudaEncoder.cpp)
    list(APPEND EXTRA_CLIENT_SOURCE_FILES src/client/decoders/nvidia/CudaDecoder.cpp) # Added to Client
    add_compile_definitions(HAVE_CUDA)
endif()

if(LIBVA_FOUND)
    message(STATUS "🔵 Intel QSV Toolkit found: Enabling QSV support...")
    list(APPEND SOURCE_FILES src/encoders/intel/IntelIrisEncoder.cpp) # Added to Engine
    list(APPEND EXTRA_CLIENT_SOURCE_FILES src/client/decoders/intel/IntelIrisDecoder.cpp)
    add_compile_definitions(HAVE_INTEL_QSV)
endif()

if(NOT CUDAToolkit_FOUND AND NOT LIBVA_FOUND)
    message(WARNING "\n⚠️ No hardware Toolkit found. No hardware acceleration will be used. Try installing the Toolkits for your system (Intel, CUDA, Apple Silicon)\nDefaulting to software encoding/decoding")
    list(APPEND SOURCE_FILES src/encoders/software/SoftwareEncoder.cpp) # Added to Engine
    list(APPEND EXTRA_CLIENT_SOURCE_FILES src/client/decoders/software/SoftwareDecoder.cpp)
endif()

# --- EXECUTABLE: CORE ENGINE ---
add_executable(core-engine ${SOURCE_FILES}
    src/encoders/software/SoftwareEncoder.cpp)
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
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ENV{PIPEWIRE_DEBUG} 4)
    endif()
    target_link_libraries(core-engine PRIVATE 
        ${PIPEWIRE_LIBRARIES} 
        ${FFMPEG_LIBRARIES} 
        Qt6::Core 
        Qt6::DBus
    )
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        message(STATUS "ARM Architecture detected. Enabling Hardware Decoder.")
    else()
        message(STATUS "x86 Architecture detected. Falling back to Software Decoder.")
    endif()

    list(APPEND SOURCE_FILES src/encoders/AppleEncoder.cpp)
    list(APPEND EXTRA_CLIENT_SOURCE_FILES src/client/decoders/AppleDecoder.cpp)
    add_compile_definitions(HAVE_APPLE_VT)

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
    src/client/Renderer.cpp
    src/client/network/ClientWebRtcManager.cpp
    src/client/decoders/software/SoftwareDecoder.cpp
    src/client/decoders/DecoderFactory.cpp 
    ${EXTRA_CLIENT_SOURCE_FILES}
)

add_executable(native-client ${CLIENT_SOURCE_FILES})
target_compile_options(native-client PRIVATE -std=c++26 -O3 -Wall -Wextra)

target_link_libraries(native-client PRIVATE 
    SDL3::SDL3 
    datachannel 
    nlohmann_json::nlohmann_json
)

if(CUDAToolkit_FOUND)                                                              
    message(STATUS "🟢 Enabling CUDA Decoder for client...")   
    list(APPEND CLIENT_SOURCE_FILES src/client/decoders/nvidia/CudaDecoder.cpp)    
    target_include_directories(native-client PRIVATE ${CUDAToolkit_INCLUDE_DIRS})  
    target_compile_definitions(native-client PRIVATE HAVE_CUDA)
    target_link_libraries(native-client PRIVATE CUDA::cudart)
endif()    

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(native-client PRIVATE ${FFMPEG_LIBRARIES})
endif()

# --- LINK HARDWARE ACCELERATION LIBRARIES TO BOTH TARGETS ---
if(CUDAToolkit_FOUND)
    # Core Engine Links
    target_include_directories(core-engine PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
    target_link_libraries(core-engine PRIVATE CUDA::cudart)
    
    # Native Client Links
    target_include_directories(native-client PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
    target_link_libraries(native-client PRIVATE CUDA::cudart)
endif()

if(LIBVA_FOUND)
    # Core Engine Links
    target_include_directories(core-engine PRIVATE ${LIBVA_INCLUDE_DIRS})
    target_link_libraries(core-engine PRIVATE ${LIBVA_LIBRARIES})
    
    # Native Client Links
    target_include_directories(native-client PRIVATE ${LIBVA_INCLUDE_DIRS})
    target_link_libraries(native-client PRIVATE ${LIBVA_LIBRARIES})
endif()