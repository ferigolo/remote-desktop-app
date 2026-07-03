#include <string>
#include <unistd.h>
#include "ScreenCapturer.hpp"
#include "LinuxCapturer.hpp"
#include "linux/ScreencastPortal.hpp"
#include "linux/PipeWireClient.hpp"

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
  pwStream = std::make_unique<PipeWireClient>();
  auto result = sp.getResult();
  bool connected = pwStream->connect(result.fd, result.node_id, on_frame_received);
  
  if (result.fd != -1)
  {
    close(result.fd);
  }

  if (!connected)
  {
    return false;
  }

  // TODO: 3. Iniciar o loop de receção de frames e chamar on_frame_received()

  return true;
}

void LinuxCapturer::stop()
{
  std::println("🐧 [LinuxCapturer] Capture stoped");
}
