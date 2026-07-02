// This will only be compiled on Ubuntu

#include "ScreenCapturer.hpp"
#include <print>

class LinuxCapturer : public ScreenCapturer
{
public:
  LinuxCapturer() { std::println("🐧 [LinuxCapturer] Instantiated. Preparing XDG Portal and PipeWire..."); }

  ~LinuxCapturer() override { stop(); }

  bool start(std::function<void(const VideoFrame &)> on_frame_received) override
  {
    std::println("🐧 [LinuxCapturer] Starting Wayland capture process...");

    // TODO: 1. Comunicar com D-Bus para abrir o Pop-up de permissão
    // TODO: 2. Conectar ao PipeWire usando o File Descriptor recebido
    // TODO: 3. Iniciar o loop de receção de frames e chamar on_frame_received()

    return true;
  }

  void stop() override
  {
    std::println("🐧 [LinuxCapturer] Capture stoped");
  }
};

// Factory implementation
#ifdef __linux__
std::unique_ptr<ScreenCapturer> ScreenCapturer::create()
{
  return std::make_unique<LinuxCapturer>();
}
#endif