#include "CoreEngine.hpp"

#include <cmath>
#include <print>
#include <string_view>
#include <thread>

#include "configurers/CoreEngineConfigurer.hpp"
#include "utils/PlatformUtils.hpp"

#ifdef HAVE_CUDA
#include "encoders/nvidia/CudaEncoder.hpp"
#endif

CoreEngine::CoreEngine()
    : window(nullptr), renderer(nullptr), is_running(false) {}

CoreEngine::~CoreEngine() { cleanup(); }

bool CoreEngine::initialize() {
  std::println("🚀 [Core] Starting Headless Remote Desktop Host...");

  initializeWebRtcManager();

#ifdef HAVE_CUDA
  encoder = std::make_unique<CudaEncoder>(nullptr);
#else
  std::println(stderr, "❌ [Core] No hardware encoder available on this system!");
  // Throwing an exception or initializing a software encoder would go here
#endif

  capturer = ScreenCapturer::create();
  capturer->start(
      [this](const VideoFrame& frame) { handleIncomingFrame(frame); });

  is_running = true;
  renderLoop();

  return true;
}

void CoreEngine::handleIncomingFrame(const VideoFrame& frame) {
  if (!encoder->IsInitialized()) {
    if (webRtcManager) webRtcManager->setVideoFps(frame.fps);

    encoder->initialize(frame.width, frame.height, frame.fps);
    encoder->onEncodedPacketCallback = [this](AVPacket* p) {
      if (webRtcManager) webRtcManager->sendVideoPacket(p->data, p->size);
    };
  }

  if (frame.type == FrameMemoryType::DmaBuf)
    encoder->encode(frame.fd, frame.width, frame.height, frame.stride,
                    frame.drmModifier, frame.spaFormat);
  else
    encoder->encode(frame.fd, frame.width, frame.height, frame.stride,
                    frame.spaFormat);
}

void CoreEngine::initializeWebRtcManager() {
  // Instantiate signaling server
  webRtcManager = std::make_unique<WebRtcManager>();
  webRtcManager->connectSignaling("ws://0.0.0.0:8080");
  webRtcManager->initializePeerConnection();
  webRtcManager->initializeDataChannel();
  webRtcManager->initializeVideoTrack();
}

void CoreEngine::renderLoop() {
  while (is_running)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void CoreEngine::cleanup() {
  if (capturer) capturer.reset();
  if (encoder) encoder.reset();
  if (webRtcManager) webRtcManager.reset();

  std::println("🧹 [Core] Released all resources");
}

void CoreEngine::printRendererInfo() const {
#ifndef NDEBUG
  if (!renderer) return;

  const char* backendName = SDL_GetRendererName(renderer);
  std::println(" [Core] Using {} backend", backendName);
  PlatformUtils::printGPUName(renderer, backendName);
#endif
}