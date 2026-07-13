#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <print>

#include "../../encoders/BaseEncoder.hpp"
#include "../ScreenCapturer.hpp"

class PipeWireClient {
 public:
  PipeWireClient();
  ~PipeWireClient();

  bool connect(int fd, uint32_t node_id,
               std::function<void(const VideoFrame&)> callback);
  void disconnect();

  bool startLoop();
  bool configureCore();
  void configureStream();
  std::vector<uint64_t> getModifiers();
  void negotiateFormat(uint32_t node_id);
  void onConfigureEnd();

 private:
  int fd;
  uint32_t width{0}, height{0}, fps{60};
  spa_video_format format;
  uint64_t drmModifier{0};
  pw_stream* stream{};

  pw_thread_loop* loop{};
  pw_context* context{};
  pw_core* core{};

  std::function<void(const VideoFrame&)>
      onFrameCallback;  // Tells what to do with each frame. Called on every
                        // frame

  spa_hook coreListener;
  spa_hook streamListener;

  static void onCoreError(void* data, uint32_t id, int seq, int res,
                          const char* message);
  static void onStreamStateChanged(void* data, pw_stream_state old_state,
                                   pw_stream_state state, const char* error);
  static void onStreamParamChanged(void* data, uint32_t id,
                                   const struct spa_pod* param);
  static void onStreamProcess(void* data);

  pw_core_events coreEvents = {};
  pw_stream_events streamEvents = {};
};