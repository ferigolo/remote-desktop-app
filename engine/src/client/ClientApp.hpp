#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "decoders/BaseDecoder.hpp"

class ClientWebRtcManager;
class SoftwareDecoder;

class ClientApp {
 public:
  ClientApp();
  ~ClientApp();

  bool initialize(const std::string& signalingUrl);
  void run();

 private:
  void renderFrame(const uint8_t* rgbaData, int width, int height, int pitch);
  std::unique_ptr<BaseDecoder> createOptimalDecoder() {

  };

  SDL_Window* window{};
  SDL_Renderer* renderer{};
  SDL_Texture* texture{};
  int texWidth = 0;
  int texHeight = 0;

  std::unique_ptr<ClientWebRtcManager> rtcManager;
  std::unique_ptr<SoftwareDecoder> decoder;
  bool isRunning = false;

  // Cross-thread rendering synchronization
  std::mutex frameMutex;
  std::vector<uint8_t> frameBuffer;
  int framePitch = 0;
  bool frameReady = false;
};
