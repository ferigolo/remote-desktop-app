#pragma once
// This will only be compiled on Ubuntu

#include <print>
#include <memory>
#include "ScreenCapturer.hpp"

class PipeWireClient;

class LinuxCapturer : public ScreenCapturer
{
public:
  LinuxCapturer() { std::println("🐧 [LinuxCapturer] Instantiated. Preparing XDG Portal and PipeWire..."); }
  ~LinuxCapturer() override { stop(); }

  bool start(std::function<void(const VideoFrame &)> onFrameReceived) override;
  void stop() override;

private:
  std::unique_ptr<PipeWireClient> pwStream;
};

// Factory implementation
#ifdef __linux__
std::unique_ptr<ScreenCapturer> ScreenCapturer::create()
{
  return std::make_unique<LinuxCapturer>();
}
#endif