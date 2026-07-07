#pragma once
#include <sys/mman.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <print>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/opt.h>
#include <libdrm/drm_fourcc.h>
}

class BaseEncoder {
 protected:
  int fps = 60;
  bool isInitialized = false;
  AVCodecContext* codecCtx;

  virtual void processPacket(const AVFrame* frame) = 0;
  virtual void flush() = 0;
  virtual void cleanup() = 0;
  bool printAVErrorAndReturn(const char* message) {
    std::println(std::cerr, "[Encoder] {}", message);
    return false;
  }

 public:
  BaseEncoder() = default;
  BaseEncoder(std::function<void(AVPacket* pkt)> onEncodedPacketCallback)
      : onEncodedPacketCallback(std::move(onEncodedPacketCallback)) {}

  virtual ~BaseEncoder() {}

  std::function<void(AVPacket* pkt)> onEncodedPacketCallback;

  virtual bool initialize(int width, int height, int fps = 60,
                          int bitrate = 50000000) = 0;

  virtual void encode(int fd, int width, int height, int stride,
                      uint64_t modifier, uint32_t spaFormat) = 0;
  virtual void encode(int fd, int width, int height, int stride,
                      uint32_t spaFormat) = 0;

  bool IsInitialized() { return isInitialized; }
};

constexpr uint32_t getDrmFormatFromSpa(uint32_t spaFormat) {
  // Assuming these SPA constants are defined or include
  // <spa/param/video/format.h>
  switch (spaFormat) {
    case 5:  // SPA_VIDEO_FORMAT_RGBx
      return DRM_FORMAT_XBGR8888;
    case 9:  // SPA_VIDEO_FORMAT_BGRx
      return DRM_FORMAT_XRGB8888;
    case 4:  // SPA_VIDEO_FORMAT_RGBA
      return DRM_FORMAT_ABGR8888;
    default:
      return DRM_FORMAT_XRGB8888;  // Fallback
  }
}