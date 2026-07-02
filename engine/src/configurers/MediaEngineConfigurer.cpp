#include "MediaEngineConfigurer.hpp"
#include <SDL3/SDL_hints.h>

void MediaEngineConfigurer::configSDL()
{
#ifdef __linux__
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#elif defined(__APPLE__)

#endif
}