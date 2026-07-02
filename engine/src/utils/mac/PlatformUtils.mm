#include "../PlatformUtils.hpp"
#include <SDL3/SDL_metal.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include <print>
#include <string_view>

namespace PlatformUtils
{
    void printGPUName(SDL_Renderer* renderer, const char* backendName)
    {
        std::string_view backend(backendName);

        if (backend == "metal")
        {
            SDL_Window* window = SDL_GetRenderWindow(renderer);
            if (!window) return;

            SDL_MetalView view = SDL_Metal_CreateView(window);
            CAMetalLayer *layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(view);

            if (layer && layer.device)
            {
                id<MTLDevice> device = layer.device;
                const char *gpuName = [device.name UTF8String];
                std::println("🍎 GPU mapped (Metal): {}", gpuName);
            }

            SDL_Metal_DestroyView(view);
        }
    }
}
