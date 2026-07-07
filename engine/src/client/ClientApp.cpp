#include "ClientApp.hpp"

#include <cstring>
#include <iostream>
#include <print>

#include "decoders/DecoderFactory.hpp"
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

  decoder = createOptimalDecoder();
  if (!decoder->initialize()) {
    std::println("Failed to initialize H264 Decoder");
    return false;
  }

  decoder->onFrameDecoded = [this](const uint8_t* yPlane, int yPitch,
                                   const uint8_t* uvPlane, int uvPitch,
                                   int width, int height) {
    this->renderFrame(yPlane, yPitch, uvPlane, uvPitch, width, height);
  };

  rtcManager = std::make_unique<ClientWebRtcManager>();
  rtcManager->onVideoPacketReceived = [this](const uint8_t* data, size_t size) {
    if (decoder) decoder->decode(data, size);
  };

  rtcManager->connectSignaling(signalingUrl);
  return true;
}

void ClientApp::renderFrame(const uint8_t* yPlane, int yPitch,
                            const uint8_t* uvPlane, int uvPitch, int width,
                            int height) {
  std::lock_guard<std::mutex> lock(frameMutex);
  texWidth = width;
  texHeight = height;
  framePitchY = yPitch;
  framePitchUV = uvPitch;
  size_t ySize = yPitch * height;
  size_t uvSize = uvPitch * ((height + 1) / 2);
  if (frameBufferY.size() != ySize) frameBufferY.resize(ySize);
  if (frameBufferUV.size() != uvSize) frameBufferUV.resize(uvSize);
  std::memcpy(frameBufferY.data(), yPlane, ySize);
  std::memcpy(frameBufferUV.data(), uvPlane, uvSize);
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
          texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12,
                                      SDL_TEXTUREACCESS_STREAMING, texWidth,
                                      texHeight);
          currentTexWidth = texWidth;
          currentTexHeight = texHeight;
        }
        SDL_UpdateNVTexture(texture, nullptr, frameBufferY.data(), framePitchY,
                            frameBufferUV.data(), framePitchUV);
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
