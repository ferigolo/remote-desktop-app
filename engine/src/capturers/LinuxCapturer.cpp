#include "LinuxCapturer.hpp"
#include <string>
#include "linux/ScreencastPortal.hpp"

bool LinuxCapturer::start(std::function<void(const VideoFrame &)> on_frame_received)
{
  std::println("🐧 [LinuxCapturer] Starting Wayland capture process...");

  ScreencastPortal sp{};

  if (!sp.negotiateScreencast())
  {
    std::println("❌ [LinuxCapturer] Failed to negotiate screencast. User denied or error ocurred");
    return false;
  }

  if (!sp.openPipeWireRemote())
  {
    std::println("❌ [LinuxCapturer] Failed to open PipeWire");
    return false;
  }

  // PipeWire is now waiting for the client to tell him the video format required
  // PAUSED -> STREAMING
  

  // TODO: 3. Iniciar o loop de receção de frames e chamar on_frame_received()

  return true;
}

void LinuxCapturer::stop()
{
  std::println("🐧 [LinuxCapturer] Capture stoped");
}
