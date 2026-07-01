#include "api.hpp"
#include <thread>

bool start_media_engine()
{
  try
  {
    if (!g_engine)
      g_engine = new MediaEngine();

    // Unlocks Tauri UI imediatelly
    std::thread([]()
                { g_engine->initialize(); }) // Opens a new window accelerated by the hardware (GPU) and starts the render loop
        .detach();
    
    return true;
  }
  catch (const std::exception &e)
  {
    std::println(stderr, "[C++26 Core] Fatal error: {}", e.what());
    return false;
  }
}
