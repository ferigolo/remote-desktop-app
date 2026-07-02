#include "../PlatformUtils.hpp"
#include <SDL3/SDL_opengl.h>
#include <print>
#include <string_view>

namespace PlatformUtils
{
    void printGPUName(SDL_Renderer *renderer, const char *backendName)
    {
        (void)renderer; // To avoid unused parameter warnings

        std::string_view backend(backendName);
        if (backend == "opengl")
        {
            auto glGetStringFunc = (const GLubyte *(*)(GLenum))SDL_GL_GetProcAddress("glGetString");
            if (glGetStringFunc)
            {
                const char *gpuName = reinterpret_cast<const char *>(glGetStringFunc(GL_RENDERER));
                const char *glVersion = reinterpret_cast<const char *>(glGetStringFunc(GL_VERSION));

                std::println("\n🎮 Using GPU: {}", gpuName ? gpuName : "Unknown");
                std::println("⚙️  OpenGL version: {}\n", glVersion ? glVersion : "Unknown");
            }
        }
    }
}
