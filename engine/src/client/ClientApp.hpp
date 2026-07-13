#pragma once

#include <memory>
#include <string>

#include "Renderer.hpp"
#include "decoders/BaseDecoder.hpp"

extern "C" {
#include <SDL3/SDL.h>
}

class ClientWebRtcManager;

class ClientApp {
 public:
  ClientApp();
  ~ClientApp();

  bool initialize(const std::string& signalingUrl);
  void run();

 private:
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<ClientWebRtcManager> rtcManager;
  std::unique_ptr<BaseDecoder> decoder;
  bool isRunning = false;
};
