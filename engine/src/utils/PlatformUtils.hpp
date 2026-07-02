#pragma once
#include <SDL3/SDL.h>

namespace PlatformUtils
{
    /**
     * Extracts and prints the specific GPU name (e.g. "NVIDIA RTX 3060") 
     * based on the underlying native graphics API (OpenGL on Linux, Metal on Mac).
     */
    void printGPUName(SDL_Renderer* renderer, const char* backendName);
}
