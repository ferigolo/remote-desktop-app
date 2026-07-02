#include "LinuxCapturer.hpp"
#include <string>
#include "linux/ScreencastPortal.hpp"

bool LinuxCapturer::start(std::function<void(const VideoFrame &)> on_frame_received)
{
  std::println("🐧 [LinuxCapturer] Starting Wayland capture process...");

  ScreencastPortal sp{};
  ScreencastResult result = sp.negotiateScreencast();

  if (!result.success)
  {
    std::println("❌ [LinuxCapturer] Failed to negotiate screencast. User denied or error ocurred");
    return false;
  }

  std::println("✅ [LinuxCapturer] Screencast acknowledged with Node ID {}", result.node_id);

  // TODO: 2. Conectar ao PipeWire usando o File Descriptor recebido
  // TODO: 3. Iniciar o loop de receção de frames e chamar on_frame_received()

  return true;
}

void LinuxCapturer::stop()
{
  std::println("🐧 [LinuxCapturer] Capture stoped");
}
