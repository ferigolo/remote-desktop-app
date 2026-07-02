#include "CoreEngine.hpp"
#include <print>
#include <exception>
#include <cstdio>

int main()
{
    // Disable stdout buffering so prints are sent immediately over the IPC pipe to Rust
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    try
    {
        std::println(" [Engine] Starting standalone MediaEngine...");

        MediaEngine engine;
        if (!engine.initialize())
        {
            std::println(stderr, "❌ [ Engine] Failed to initialize MediaEngine");
            return 1;
        }

        std::println("✅ [Engine] Engine terminated gracefully.");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::println(stderr, "❌ [Engine] Fatal error: {}", e.what());
        return 1;
    }
}
