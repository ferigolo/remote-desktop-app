#pragma once
#include <SDL3/SDL.h>
#include <atomic>
#include "capturers/ScreenCapturer.hpp"
#include "network/WebRtcManager.hpp"

class CoreEngine
{
public:
  CoreEngine();
  ~CoreEngine();

  bool initialize();

private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  std::atomic<bool> is_running;
  std::unique_ptr<ScreenCapturer> capturer;
  std::unique_ptr<WebRtcManager> webRtcManager;

  void renderLoop();
  void cleanup();
  void printRendererInfo() const;
};