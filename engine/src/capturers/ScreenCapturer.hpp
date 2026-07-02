#pragma once
#include <cstdint>
#include <functional>
#include <memory>

struct VideoFrame
{
  uint8_t *data;
  int width, height, stride;
};

// Any capturer (Linux, Mac, Windows) will have to follow this interface
class ScreenCapturer
{
public:
  virtual ~ScreenCapturer() = default;

  // This callback will be called on every new frame arrives on the GPU
  virtual bool start(std::function<void(const VideoFrame &)> on_frame_received) = 0;
  virtual void stop() = 0;

  static std::unique_ptr<ScreenCapturer> create();
};