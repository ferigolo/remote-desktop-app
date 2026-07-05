#pragma once
#include <SDL3/SDL.h>
#include <atomic>
#include "capturers/ScreenCapturer.hpp"
#include "network/WebRtcManager.hpp"
#include "encoders/H264Encoder.hpp"

class CoreEngine
{
public:
  CoreEngine();
  ~CoreEngine();

  bool initialize();
  void initializeWebRtcManager();

private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  std::atomic<bool> is_running;
  std::unique_ptr<H264Encoder> encoder;
  std::unique_ptr<ScreenCapturer> capturer;
  std::unique_ptr<WebRtcManager> webRtcManager;

  void renderLoop();
  void cleanup();
  void handleIncomingFrame(const VideoFrame &frame);
  void printRendererInfo() const;
};