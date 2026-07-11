#pragma once
#include <SDL3/SDL.h>

#include <atomic>

#include "capturers/ScreenCapturer.hpp"
#include "encoders/BaseEncoder.hpp"
#include "network/WebRtcManager.hpp"

extern "C" {
#include <SDL3/SDL.h>
}

class CoreEngine {
 public:
  CoreEngine();
  ~CoreEngine();

  bool initialize();
  void initializeWebRtcManager();

 private:
  SDL_Window* window;
  SDL_Renderer* renderer;
  std::atomic<bool> is_running;
  std::unique_ptr<BaseEncoder> encoder;
  std::unique_ptr<ScreenCapturer> capturer;
  std::unique_ptr<WebRtcManager> webRtcManager;

  void renderLoop();
  void cleanup();
  void handleIncomingFrame(const VideoFrame& frame);
  void printRendererInfo() const;
};