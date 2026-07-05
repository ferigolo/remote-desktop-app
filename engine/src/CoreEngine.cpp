#include "CoreEngine.hpp"
#include "configurers/CoreEngineConfigurer.hpp"
#include "utils/PlatformUtils.hpp"
#include <SDL3/SDL.h>
#include <print>
#include <thread>
#include <cmath>
#include <string_view>

CoreEngine::CoreEngine() : window(nullptr), renderer(nullptr), is_running(false) {}

CoreEngine::~CoreEngine()
{
  cleanup();
}

bool CoreEngine::initialize()
{
  CoreEngineConfigurer::configSDL();

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::println(stderr, "❌ [CoreEngine] Error initializing SDL: {}", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("Remote Desktop", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window)
  {
    std::println(stderr, "❌ [CoreEngine] Error creating window: {}", SDL_GetError());
    cleanup();
    return false;
  }

  initializeWebRtcManager();

  // SDL3 no longer uses flags for hardware acceleration/software in CreateRenderer.
  // It handles it natively or you pass a specific driver string. Passing NULL gets the best default.
  renderer = SDL_CreateRenderer(window, NULL);
  SDL_SetRenderVSync(renderer, 1);

  if (!renderer)
  {
    std::println(stderr, "❌ [CoreEngine] Error creating renderer: {}", SDL_GetError());
    cleanup();
    return false;
  }
  std::println(" [Core] Renderer created successfully");
  printRendererInfo();
  is_running = true;

  encoder = std::make_unique<H264Encoder>();
  capturer = ScreenCapturer::create();
  capturer->start([this](const VideoFrame &frame)
                  { handleIncomingFrame(frame); });

  renderLoop();

  return true;
}

void CoreEngine::handleIncomingFrame(const VideoFrame &frame)
{
  if (!encoder->isInitialized)
  {
    encoder->initialize(frame.width, frame.height, frame.fps);
    encoder->onEncodedPacketCallback = [this](AVPacket *p)
    {
      if (webRtcManager)
        webRtcManager->sendVideoPacket(p->data, p->size);
    };
  }

  if (frame.type == FrameMemoryType::DmaBuf)
    encoder->encodeDmaBuf(frame.fd, frame.width, frame.height, frame.stride, frame.drmModifier);
  else
    encoder->encodeMemFd(frame.fd, frame.width, frame.height, frame.stride);
}

void CoreEngine::initializeWebRtcManager()
{
  // Instantiate signaling server
  webRtcManager = std::make_unique<WebRtcManager>();
  webRtcManager->connectSignaling("ws://0.0.0.0:8080");
  webRtcManager->initializePeerConnection();
  webRtcManager->initializeDataChannel();
  webRtcManager->initializeVideoTrack();
}

void CoreEngine::renderLoop()
{
  SDL_Event event;
  Uint32 start_time = SDL_GetTicks();

  while (is_running)
  {
    while (SDL_PollEvent(&event)) // poll until all events are handled
    {
      if (event.type == SDL_EVENT_QUIT)
      {
        is_running = false;
        std::println("🛑 [CoreEngine] Quit window signal received");
      }
      // TODO: capture mouse and keyboard events
    }

    Uint32 current_time = SDL_GetTicks() - start_time;
    Uint8 red = static_cast<Uint8>((std::sin(current_time * 0.001) + 1.0) * 127.5);
    Uint8 green = static_cast<Uint8>((std::sin(current_time * 0.002) + 1.0) * 127.5);
    Uint8 blue = static_cast<Uint8>((std::sin(current_time * 0.003) + 1.0) * 127.5);

    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    SDL_RenderClear(renderer);

    SDL_RenderPresent(renderer);
  }

  cleanup();
}

void CoreEngine::cleanup()
{
  if (capturer)
    capturer.reset();
  if (encoder)
    encoder.reset();
  if (webRtcManager)
    webRtcManager.reset();
  if (renderer)
  {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window)
  {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  if (SDL_WasInit(0) != 0)
  {
    SDL_Quit();
    std::println("🧹 [CoreEngine] Released resources");
  }
}

void CoreEngine::printRendererInfo() const
{
#ifndef NDEBUG
  if (!renderer)
    return;

  const char *backendName = SDL_GetRendererName(renderer);
  std::println(" [Core] Using {} backend", backendName);

  // Delegando o trabalho sujo e OS-specific para as Utils
  PlatformUtils::printGPUName(renderer, backendName);
#endif
}