#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class BaseDecoder {
 public:
  virtual ~BaseDecoder() = default;

  virtual bool initialize() = 0;
  virtual void decode(const uint8_t* data, size_t size) = 0;
  std::function<void(const uint8_t* rgbaData, int width, int height, int pitch)>
      onFrameDecoded;

 protected:
  bool isInitialized = false;
  bool hasDecodedFirstFrame = false;

  AVCodecContext* codecCtx{};

  bool printAvErrorAndReturn(std::string_view contextMessage) {
    std::println(std::cerr, "❌ [Decoder] {} ", contextMessage);
    return false;
  }
  bool printAvErrorAndReturn(int ret, std::string_view contextMessage) {
    if (ret < 0) {
      char errbuf[AV_ERROR_MAX_STRING_SIZE];
      // Translate the error code to a readable string
      if (av_strerror(ret, errbuf, sizeof(errbuf)) < 0)
        snprintf(errbuf, sizeof(errbuf), "Unknown error");

      std::println(std::cerr, "❌ [Decoder] {} | FFmpeg error: {} (Code: {})",
                   contextMessage, errbuf, ret);
    }
    return false;
  }
};
