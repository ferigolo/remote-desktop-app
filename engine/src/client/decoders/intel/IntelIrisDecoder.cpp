#include "IntelIrisDecoder.hpp"

#include <print>

extern "C" {
#include <libavcodec/codec.h>
#include <libavutil/hwcontext_drm.h>
}

IntelIrisDecoder::IntelIrisDecoder() {}

IntelIrisDecoder::~IntelIrisDecoder() {}

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
  if (int ret = av_hwdevice_ctx_create(
          &deviceCtx, AV_HWDEVICE_TYPE_QSV,
          "/dev/dri/renderD128",  // Explicitly point to the Intel iGPU node
          nullptr, 0);
      ret < 0)
    return printAvErrorAndReturn(ret,
                                 "Failed to create Intel QSV hardware context. "
                                 "Check Intel Media drivers.");
  // Attach hardware context to the decoder
  codecCtx->hw_device_ctx = av_buffer_ref(deviceCtx);
  if (int ret = avcodec_open2(codecCtx, codec, nullptr); ret < 0)
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

void IntelIrisDecoder::decode(const uint8_t* data, size_t size) {
  if (!isInitialized) return;

  AVPacket* pkt = av_packet_alloc();
  pkt->data = (uint8_t*)data;
  pkt->size = size;

  if (int ret = avcodec_send_packet(codecCtx, pkt); ret < 0) {
    if (hasDecodedFirstFrame) {
      char errbuf[128];
      av_strerror(ret, errbuf, sizeof(errbuf));
      printAvErrorAndReturn(ret, "avcodec_send_packet failed");
    } else
      while (true) {
        // Extract the decoded hardware frame from the GPU
        int ret = avcodec_receive_frame(codecCtx, hwFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        else if (ret < 0) {
          if (hasDecodedFirstFrame) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            printAvErrorAndReturn(
                std::format("avcodec_receive_frame failed: {}", errbuf));
          }
          break;
        }

        if (!hasDecodedFirstFrame) {
          hasDecodedFirstFrame = true;
          av_log_set_level(AV_LOG_WARNING);  // Restore error logs
        }

        // Allocate a DRM PRIME frame
        AVFrame* drmFrame = av_frame_alloc();
        drmFrame->format = AV_PIX_FMT_DRM_PRIME;

        if (int ret = av_hwframe_map(drmFrame, hwFrame, AV_HWFRAME_MAP_READ);
            ret < 0) {
          printAvErrorAndReturn(ret, "Failed mapping hwFrame into drmFrame");
          continue;
        }
        // You now have the raw File Descriptor (FD) pointing to the VRAM!

        if (int ret = av_hwframe_transfer_data(swFrame, hwFrame, 0); ret < 0) {
          printAvErrorAndReturn(ret, "Error transfering data from VRAM to RAM");
          continue;
        }

        static int frameCount = 0;
        if (frameCount++ % 60 == 0)
          std::println(
              "✅ [Decoder] Successfully decoded frame #{} ({}x{}) - Format: "
              "NV12",
              frameCount, swFrame->width, swFrame->height);

        if (onFrameDecoded) onFrameDecoded(swFrame);
      }
  }
  av_packet_free(&pkt);
}

void IntelIrisDecoder::updateTexture(SDL_Renderer* renderer,
                                     SDL_Texture** texture) {
  (void)renderer;
  (void)texture;
}
