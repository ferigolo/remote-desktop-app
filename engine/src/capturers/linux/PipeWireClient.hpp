#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <print>
#include <functional>
#include "../ScreenCapturer.hpp"

class PipeWireClient
{
public:
  PipeWireClient();
  ~PipeWireClient();

  bool connect(int fd, uint32_t node_id, std::function<void(const VideoFrame &)> callback);
  void disconnect();

  bool startLoop();
  bool configureCore();
  void configureStream();
  void negotiateFormat(uint32_t node_id);
  void onConfigureEnd();

private:
  int fd;
  pw_thread_loop *loop{};
  pw_context *context{};
  pw_core *core{};
  pw_stream *stream{};

  std::function<void(const VideoFrame &)> onFrameCallback; // Tells what to do with each frame. Called on every frame

  spa_hook core_listener;
  spa_hook stream_listener;

  static void onCoreError(void *data, uint32_t id, int seq, int res, const char *message);
  static void onStreamStateChanged(void *data, pw_stream_state old_state, pw_stream_state state, const char *error);
  static void onStreamParamChanged(void *data, uint32_t id, const struct spa_pod *param);

  static void onStreamProcess(void *data);

  pw_core_events core_events = {};
  pw_stream_events stream_events = {};
};