#include "api.hpp"
#include <thread>

bool start_media_engine(EventCallback onCloseCallback)
{
  try
  {
    if (!gEngine)
      gEngine = new MediaEngine();

    // Unlocks Tauri UI imediatelly
    std::thread([onCloseCallback]()
                { gEngine->initialize();
                onCloseCallback(); }) // Opens a new window accelerated by the hardware (GPU) and starts the render loop
        .detach();

    return true;
  }
  catch (const std::exception &e)
  {
    std::println(stderr, "[C++26 Core] Fatal error: {}", e.what());
    return false;
  }
}
