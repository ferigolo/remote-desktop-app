#include "ClientApp.hpp"

#include <cstring>
#include <iostream>
#include <print>
#include <utility>

#include "decoders/DecoderFactory.hpp"
#include "network/ClientWebRtcManager.hpp"

ClientApp::ClientApp() {}

ClientApp::~ClientApp() { SDL_Quit(); }

bool ClientApp::initialize(const std::string& signalingUrl) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println("SDL Init failed: {}", SDL_GetError());
    return false;
  }

  renderer = std::make_unique<Renderer>();

  decoder = createOptimalDecoder();
  if (!decoder->initialize()) {
    std::println(stderr, "Failed to initialize H264 Decoder");
    return false;
  }
  decoder->onFrameDecoded = [this](AVFrame* frame) {
    this->renderer->setFrame(frame);
  };

  rtcManager = std::make_unique<ClientWebRtcManager>();
  rtcManager->onVideoPacketReceived = [this](const uint8_t* data, size_t size) {
    if (decoder) decoder->decode(data, size);
  };
  rtcManager->onResolutionSetCallback = [this](int width, int height) {
    renderer->setHostResolution(width, height);
  };
  rtcManager->connectSignaling(signalingUrl);

  if (!renderer->initialize()) {
    std::println(stderr, "Failed to initialize Renderer");
    return false;
  }

  return true;
}

void ClientApp::run() {
  isRunning = true;
  SDL_Event event;

  while (isRunning) {
    while (SDL_PollEvent(&event))
      if (event.type == SDL_EVENT_QUIT) isRunning = false;

    if (renderer) renderer->renderFrame();

    SDL_Delay(5);  // Keep CPU usage low in main thread
  }
}
