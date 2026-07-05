#pragma once
#include <cstdint>
#include <functional>
#include <memory>

enum class FrameMemoryType {                                                             
  DmaBuf, // Memory directly on VRAM (GPU)
  MemFd   // Memory shared on RAM (CPU)                                      
};

struct VideoFrame
{
  FrameMemoryType type;

  int fd;
  uint32_t width;
  uint32_t height;
  uint32_t stride; // Largura da linha em bytes (importante para o FFmpeg)
  uint32_t fps;

  uint64_t drmModifier;
};

// Any capturer (Linux, Mac, Windows) will have to follow this interface
class ScreenCapturer
{
public:
  virtual ~ScreenCapturer() = default;

  // This callback will be called on every new frame arrives on the GPU
  virtual bool start(std::function<void(const VideoFrame &)> onFrameReceived) = 0;
  virtual void stop() = 0;

  static std::unique_ptr<ScreenCapturer> create();
};