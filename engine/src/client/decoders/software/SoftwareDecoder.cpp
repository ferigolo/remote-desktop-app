#include "SoftwareDecoder.hpp"

#include <iostream>
#include <print>

SoftwareDecoder::SoftwareDecoder() {}

SoftwareDecoder::~SoftwareDecoder() {
  if (swsCtx) sws_freeContext(swsCtx);
  if (frame) av_frame_free(&frame);
  if (swFrame) av_frame_free(&swFrame);
  if (codecCtx) avcodec_free_context(&codecCtx);
}

bool SoftwareDecoder::initialize() {
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) return printAvErrorAndReturn(" Software H264 decoder not found");
  std::println(
      "⚠️ [Decoder] Hardware decoder unavailable. Falling back to Software "
      "Decoder");

  codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx) return false;

  // Optional: enable multithreaded decoding
  codecCtx->thread_count = 4;
  codecCtx->thread_type = FF_THREAD_SLICE;

  if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
    std::println("Failed to open H264 decoder");
    return false;
  }

  frame = av_frame_alloc();
  swFrame = av_frame_alloc();
  isInitialized = true;

  // Omits "PPS 0 referenced" FFmpeg logs until get the IDR frame.
  av_log_set_level(AV_LOG_QUIET);

  std::println("✅ [Decoder] H264 Decoder Initialized");
  return true;
}

void SoftwareDecoder::decode(const uint8_t* data, size_t size) {
  if (!isInitialized) return;

  AVPacket* pkt = av_packet_alloc();
  pkt->data = (uint8_t*)data;
  pkt->size = size;

  if (int ret = avcodec_send_packet(codecCtx, pkt); ret < 0) {
    if (hasDecodedFirstFrame) {
      char errbuf[128];
      av_strerror(ret, errbuf, sizeof(errbuf));
      std::println("❌ [Decoder] avcodec_send_packet failed: {}", errbuf);
    }
  } else {
    while (true) {
      ret = avcodec_receive_frame(codecCtx, frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        if (hasDecodedFirstFrame) {
          char errbuf[128];
          av_strerror(ret, errbuf, sizeof(errbuf));
          std::println("❌ [Decoder] avcodec_receive_frame failed: {}", errbuf);
        }
        break;
      }

      if (!hasDecodedFirstFrame) {
        hasDecodedFirstFrame = true;
        av_log_set_level(AV_LOG_WARNING);  // Restaura os logs de erro
      }

      // First frame received: initialize sws_scale
      if (!swsCtx) {
        swsCtx = sws_getContext(frame->width, frame->height,
                                (AVPixelFormat)frame->format, frame->width,
                                frame->height, AV_PIX_FMT_NV12,
                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

        swFrame->format = AV_PIX_FMT_NV12;
        swFrame->width = frame->width;
        swFrame->height = frame->height;
        av_image_alloc(swFrame->data, swFrame->linesize, frame->width,
                       frame->height, AV_PIX_FMT_NV12, 32);
      }

      sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
                swFrame->data, swFrame->linesize);

      static int frameCount = 0;
      if (frameCount++ % 60 == 0) {
        std::println(
            "✅ [Decoder] Successfully decoded and converted frame #{} ({}x{})",
            frameCount, frame->width, frame->height);
      }

      if (onFrameDecoded) onFrameDecoded(swFrame);
    }
  }
  av_packet_free(&pkt);
}

void SoftwareDecoder::updateTexture(SDL_Renderer* renderer, SDL_Texture** texture) {
  // Texture updates are handled by Renderer for software decoding
  (void)renderer;
  (void)texture;
}
