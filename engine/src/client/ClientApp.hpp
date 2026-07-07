#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "decoders/BaseDecoder.hpp"

class ClientWebRtcManager;

class ClientApp {
 public:
  ClientApp();
  ~ClientApp();

  bool initialize(const std::string& signalingUrl);
  void run();

 private:
  void renderFrame(const uint8_t* yPlane, int yPitch, const uint8_t* uvPlane,
                   int uvPitch, int width, int height);


  SDL_Window* window{};
  SDL_Renderer* renderer{};
  SDL_Texture* texture{};
  int texWidth = 0;
  int texHeight = 0;

  std::unique_ptr<ClientWebRtcManager> rtcManager;
  std::unique_ptr<BaseDecoder> decoder;
  bool isRunning = false;

  // Cross-thread rendering synchronization
  std::mutex frameMutex;
  std::vector<uint8_t> frameBufferY;
  std::vector<uint8_t> frameBufferUV;
  int framePitchY = 0;
  int framePitchUV = 0;
  bool frameReady = false;
};
