#include "CudaDecoder.hpp"

#include <print>

CudaDecoder::CudaDecoder() {}

CudaDecoder::~CudaDecoder() {
  if (frame) av_frame_free(&frame);
  if (codecCtx) avcodec_free_context(&codecCtx);
  if (hwDeviceCtx) av_buffer_unref(&hwDeviceCtx);
}

// Static callback to force FFmpeg to output CUDA memory frames
AVPixelFormat CudaDecoder::getHwFormat(AVCodecContext* ctx,
                                       const AVPixelFormat* pixFmts) {
  for (const AVPixelFormat* p = pixFmts; *p != -1; p++)
    if (*p == AV_PIX_FMT_CUDA) return *p;

  std::println(std::cerr, "❌ [Decoder] Failed to negotiate CUDA pixel format");
  return AV_PIX_FMT_NONE;
}

bool CudaDecoder::initialize() {
  // Find the CUDA hardware decoder
  const AVCodec* codec =
      avcodec_find_decoder_by_name("h264_cuvid");  // or hevc_cuvid
  if (!codec) return printAvErrorAndReturn("CUDA decoder not found");

  codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx)
    return printAvErrorAndReturn("Failed initializing codec context.");

  // Create the CUDA hardware device context
  if (int ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, "0",
                                       nullptr, 0) < 0)
    return printAvErrorAndReturn(
        ret, "Failed to create CUDA hardware device context");

  // Attach the hardware context and format negociation callback
  codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
  codecCtx->get_format = getHwFormat;

  // enable multithreaded decoding(helps with parsing before it hits the GPU)
  codecCtx->thread_count = 4;
  codecCtx->thread_type = FF_THREAD_SLICE;

  if (int ret = avcodec_open2(codecCtx, codec, nullptr) < 0)
    return printAvErrorAndReturn(ret, "Failed to open H264 CUDA decoder");

  frame = av_frame_alloc();
  isInitialized = true;

  av_log_set_level(AV_LOG_QUIET);  // Omit logs until the first IDR frame
  std::println(
      "✅ [Decoder] CUDA Hardware Decoder Initialized (Zero-Copy mode)");
  return true;
}

void CudaDecoder::decode(const uint8_t* data, size_t size) {
  if (!isInitialized) return;

  AVPacket* pkt = av_packet_alloc();
  pkt->data = (uint8_t*)data;
  pkt->size = size;

  if (int ret = avcodec_send_packet(codecCtx, pkt) < 0)
    if (hasDecodedFirstFrame)
      printAvErrorAndReturn(ret, "Failed sending packet into CUDA decoder");
    else
      while (true) {
        int ret = avcodec_receive_frame(codecCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        else if (ret < 0) {
          if (hasDecodedFirstFrame)
            printAvErrorAndReturn(ret,
                                  "Failed to receive decoded "
                                  "frame from codec");
          break;
        }
        
        if (!hasDecodedFirstFrame) {
          hasDecodedFirstFrame = true;
          av_log_set_level(AV_LOG_WARNING);
        }

        static int frameCount = 0;
        if (frameCount++ % 60 == 0)
          std::println("✅ [Decoder] HW Decoded frame #{} ({}x{}) - Format: {}",
                       frameCount, frame->width, frame->height,
                       av_get_pix_fmt_name((AVPixelFormat)frame->format));

        // IMPORTANT: The pointers below are CUDA device pointers (VRAM), not
        // host RAM pointers!
        // They are typically in NV12 format
        // (yPlane = frame->data[0], uvPlane = frame->data[1])

        if (onFrameDecoded)
          onFrameDecoded(frame->data[0], frame->linesize[0], frame->data[1],
                         frame->linesize[1], frame->width, frame->height);
      }

  av_packet_free(&pkt);
}
