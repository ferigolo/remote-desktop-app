#pragma once
#include <SDL3/SDL.h>
#include <atomic>
#include "capturers/ScreenCapturer.hpp"

class MediaEngine
{
public:
  MediaEngine();
  ~MediaEngine();

  bool initialize();

private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  std::atomic<bool> is_running;
  std::unique_ptr<ScreenCapturer> capturer;

  void render_loop();
  void cleanup();
  void printRendererInfo() const;
};