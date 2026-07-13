#pragma once
#include <cstdint>
#include <functional>
#include <memory>

enum class FrameMemoryType {
  DmaBuf,  // Memory directly on VRAM (GPU)
  MemFd,   // Memory shared on RAM (CPU) via fd
  MemPtr   // Memory shared on RAM (CPU) via direct pointer
};

struct VideoFrame {
  FrameMemoryType type;

  int fd;
  uint8_t* data = nullptr; // Used for MemPtr
  uint32_t size = 0;       // Used for MemPtr
  uint32_t width;
  uint32_t height;
  uint32_t stride;  // Line width in bytes
  uint32_t fps;
  uint32_t spaFormat;
  uint64_t drmModifier;  // Tells how the GPU organized the pixels on VRAM
};

// Any capturer (Linux, Mac, Windows) will have to follow this interface
class ScreenCapturer {
 public:
  virtual ~ScreenCapturer() = default;

  // This callback will be called on every new frame arrives on the GPU
  virtual bool start(
      std::function<void(const VideoFrame&)> onFrameReceived) = 0;
  virtual void stop() = 0;

  static std::unique_ptr<ScreenCapturer> create();
};