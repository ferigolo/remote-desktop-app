#include "IntelIrisEncoder.hpp"

IntelIrisEncoder::IntelIrisEncoder(
    std::function<void(AVPacket* pkt)> onEncodedPacketCallback)
    : onEncodedPacketCallback(onEncodedPacketCallback) {}

IntelIrisEncoder::~IntelIrisEncoder() { cleanup(); }

bool IntelIrisEncoder::initialize(int width, int height, int fps, int bitrate) {
  codec = avcodec_find_encoder_by_name("h264_qsv");
  if (!codec)
    return printAvErrorAndReturn("Failed to initialize h264_qsv codec");

  codecCtx = avcodec_alloc_context3(codec);
  // Set standard encoder parameters
  codecCtx->width = width;
  codecCtx->height = height;
  codecCtx->time_base = {1, fps};
  codecCtx->framerate = {fps, 1};
  codecCtx->pix_fmt = AV_PIX_FMT_QSV;

  if (int ret = av_hwdevice_ctx_create(&hwCtx, AV_HWDEVICE_TYPE_QSV, "auto",
                                       nullptr, 0) < 0)
    return printAvErrorAndReturn(ret,
                                 "Failed to create hardware device context");

  // Create hardware frames - Allocates GPU memory pool
  // The encoder needs this context linked to it before opening it
  hwFramesRef = av_hwframe_ctx_alloc(hwCtx);
  framesCtx = (AVHWFramesContext*)(hwFramesRef->data);

  // Define the hardware frame properties
  framesCtx->format = AV_PIX_FMT_QSV;  // The hardware surface format
  framesCtx->sw_format =
      AV_PIX_FMT_NV12;  // The underlying software format QSV uses
  framesCtx->width = codecCtx->width;
  framesCtx->height = codecCtx->height;
  framesCtx->initial_pool_size =
      32;  // Number of frames to pre-allocate in VRAM

  if (int ret = av_hwframe_ctx_init(hwFramesRef) < 0)
    return printAvErrorAndReturn(ret, "Failed initializing frames context");

  codecCtx->hw_frames_ctx = av_buffer_ref(hwFramesRef);

  // Now that encoder knows where its hardware memory pool is, we can open it
  if (int ret = avcodec_open2(codecCtx, codec, nullptr) < 0)
    return printAvErrorAndReturn(ret, "Failed to open the encoder");

  isInitialized = true;
  return true;
}

void IntelIrisEncoder::encode(int fd, int width, int height, int stride,
                              uint64_t modifier, uint32_t spaFormat) {}

void IntelIrisEncoder::encode(int fd, int width, int height, int stride,
                              uint32_t spaFormat) {}

void IntelIrisEncoder::processPacket(const AVFrame* frame) {}

void IntelIrisEncoder::flush() {}

void IntelIrisEncoder::cleanup() {
  if (hwFramesRef) av_buffer_unref(&hwFramesRef);
  if (hwCtx) av_buffer_unref(&hwCtx);
  if (codecCtx) avcodec_free_context(&codecCtx);
}
