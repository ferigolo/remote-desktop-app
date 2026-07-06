#include "ClientApp.hpp"

#include <cstring>
#include <iostream>
#include <print>

#include "decoders/SoftwareDecoder.hpp"
#include "network/ClientWebRtcManager.hpp"

ClientApp::ClientApp() {}

ClientApp::~ClientApp() {
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_Quit();
}

bool ClientApp::initialize(const std::string& signalingUrl) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println("SDL Init failed: {}", SDL_GetError());
    return false;
  }

  window =
      SDL_CreateWindow("Remote Desktop Client - Native", 1920, 1080,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                           SDL_WINDOW_INPUT_FOCUS);
  if (!window) return false;

  renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) return false;

  int hostWidth = 1920, hostHeight = 1080;
  if (!SDL_SetRenderLogicalPresentation(renderer, hostWidth, hostHeight,
                                        SDL_LOGICAL_PRESENTATION_LETTERBOX))
    std::println(std::cerr, "Warning: Letterboxing failed: {}", SDL_GetError());

  decoder = std::make_unique<SoftwareDecoder>();
  if (!decoder->initialize()) {
    std::println("Failed to initialize H264 Decoder");
    return false;
  }

  decoder->onFrameDecoded = [this](const uint8_t* rgbaData, int width,
                                   int height, int pitch) {
    this->renderFrame(rgbaData, width, height, pitch);
  };

  rtcManager = std::make_unique<ClientWebRtcManager>();
  rtcManager->onVideoPacketReceived = [this](const uint8_t* data, size_t size) {
    if (decoder) decoder->decode(data, size);
  };

  rtcManager->connectSignaling(signalingUrl);
  return true;
}

void ClientApp::renderFrame(const uint8_t* rgbaData, int width, int height,
                            int pitch) {
  std::lock_guard<std::mutex> lock(frameMutex);
  texWidth = width;
  texHeight = height;
  framePitch = pitch;
  size_t dataSize = pitch * height;
  if (frameBuffer.size() != dataSize) frameBuffer.resize(dataSize);
  std::memcpy(frameBuffer.data(), rgbaData, dataSize);
  frameReady = true;
}

void ClientApp::run() {
  isRunning = true;
  SDL_Event event;
  int currentTexWidth = 0;
  int currentTexHeight = 0;

  while (isRunning) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) isRunning = false;
    }

    {
      std::lock_guard<std::mutex> lock(frameMutex);
      if (frameReady) {
        if (!texture || currentTexWidth != texWidth ||
            currentTexHeight != texHeight) {
          if (texture) SDL_DestroyTexture(texture);
          texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STREAMING, texWidth,
                                      texHeight);
          currentTexWidth = texWidth;
          currentTexHeight = texHeight;
        }
        SDL_UpdateTexture(texture, nullptr, frameBuffer.data(), framePitch);
        frameReady = false;
      }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (texture) SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    SDL_Delay(5);  // Keep CPU usage low in main thread
  }
}
