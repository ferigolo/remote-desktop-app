// This will only be compiled on Ubuntu

#include <dbus/dbus.h>
#include <print>
#include "ScreenCapturer.hpp"

struct PortalResponse
{
  uint32_t response_code;
  std::string session_handle;
  uint32_t node_id;
};

class LinuxCapturer : public ScreenCapturer
{
public:
  LinuxCapturer() { std::println("🐧 [LinuxCapturer] Instantiated. Preparing XDG Portal and PipeWire..."); }
  ~LinuxCapturer() override { stop(); }

  bool start(std::function<void(const VideoFrame &)> on_frame_received) override;
  void stop() override;

private:
  DBusConnection *conn;
  std::string sessionPath;

  static void handleDBusError(DBusError *error, const char *context);
  bool requestScreencastSession();
  bool selectSources();
  bool startScreencast();
  PortalResponse waitForResponse(const std::string& requestPath);
};

// Factory implementation
#ifdef __linux__
std::unique_ptr<ScreenCapturer> ScreenCapturer::create()
{
  return std::make_unique<LinuxCapturer>();
}
#endif