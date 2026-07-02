// This will only be compiled on Ubuntu

#include <dbus/dbus.h>
#include <print>
#include "ScreenCapturer.hpp"

class LinuxCapturer : public ScreenCapturer
{
public:
  LinuxCapturer() { std::println("🐧 [LinuxCapturer] Instantiated. Preparing XDG Portal and PipeWire..."); }
  ~LinuxCapturer() override { stop(); }

  bool start(std::function<void(const VideoFrame &)> on_frame_received) override;
  void stop() override;

  static void handleDBusError(DBusError *error, const char *context);
  void requestScreencastSession();
  void selectSources();
  void startScreencast();

private:
  DBusConnection *conn;
  const char *sessionHandlePath{};
};

// Factory implementation
#ifdef __linux__
std::unique_ptr<ScreenCapturer> ScreenCapturer::create()
{
  return std::make_unique<LinuxCapturer>();
}
#endif