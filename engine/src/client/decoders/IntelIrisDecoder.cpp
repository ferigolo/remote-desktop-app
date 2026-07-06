#include "IntelIrisDecoder.hpp"

#include <libavcodec/codec.h>

#include <print>

IntelIrisDecoder::IntelIrisDecoder() {}

IntelIrisDecoder::~IntelIrisDecoder() {}

// 1. Find decoder lib
// 2. Alloc codec context

bool IntelIrisDecoder::initialize() {
  const AVCodec* codec = avcodec_find_decoder_by_name("h264_qsv");
  if (!codec)
    return printAvErrorAndReturn(
        "h264_qsv decoder not found! Make sure FFmpeg is compiled "
        "with --enable-libmfx");

  auto codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx) return false;
  codecCtx->thread_count = 1;  // Optional but recommended for low latency

  // Initialize the Intel QSV Hardware Context
  if (int ret =
          av_hwdevice_ctx_create(
              &deviceCtx, AV_HWDEVICE_TYPE_QSV,
              "/dev/dri/renderD128",  // Explicitly point to the Intel iGPU node
              nullptr, 0) < 0)
    return printAvErrorAndReturn(ret,
                                 "Failed to create Intel QSV hardware context. "
                                 "Check Intel Media drivers.");
  // Attach hardware context to the decoder
  codecCtx->hw_device_ctx = av_buffer_ref(deviceCtx);
  if (int ret = avcodec_open2(codecCtx, codec, nullptr) < 0)
    return printAvErrorAndReturn(ret, "Failed to open QSV H264 decoder.");
  
  hwFrame = av_frame_alloc();
  swFrame = av_frame_alloc();
  isInitialized = true;

  // Suppress initial FFmpeg "PPS 0 referenced" logs until the first IDR frame
  // hits
  av_log_set_level(AV_LOG_QUIET);
  std::println(
      "✅ [Decoder] Intel QSV Hardware Decoder Initialized (Zero-Copy Target: "
      "NV12)");
  return true;
}

void IntelIrisDecoder::decode(const uint8_t* data, size_t size) {}
