#include "api.hpp"

bool MediaEngine::initialize()
{
  std::println("⚙️ [C++26 Core] Graphic and Media engines initialized at main thread.");
  return true;
}

bool start_media_engine()
{
  try
  {
    return MediaEngine::initialize();
  }
  catch (const std::exception &e)
  {
    std::println(stderr, "[C++26 Core] Fatal error: {}", e.what());
    return false;
  }
}
