#include "CoreEngine.hpp"
#include <print>
#include <exception>

int main()
{
    try
    {
        std::println("🚀 [C++ Engine] Starting standalone MediaEngine...");
        
        MediaEngine engine;
        if (!engine.initialize())
        {
            std::println(stderr, "❌ [C++ Engine] Failed to initialize MediaEngine");
            return 1;
        }

        std::println("✅ [C++ Engine] Engine terminated gracefully.");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::println(stderr, "❌ [C++ Engine] Fatal error: {}", e.what());
        return 1;
    }
}
