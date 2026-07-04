// VAAPI accelerated encoder

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <print>

// FFmpeg is written in C, so we warn the compiler
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "../capturers/ScreenCapturer.hpp"

class H264Encoder
{
public:
  H264Encoder(int width, int height, int fps = 60);
  ~H264Encoder();

  bool initialize();
  bool encode(const VideoFrame &frame, std::function<void(const uint8_t *data, size_t size)> on_packet_ready);


private:
  int width, height, fps;
  int64_t frameCounter = 0;

  AVCodecContext *codecContext{};
  AVPacket *packet{};

  AVBufferRef *hwDeviceCtx{};
  AVFilterGraph *filterGraph{};
  AVFilterContext *bufSrcCtx{};
  AVFilterContext *bufSinkCtx{};

  // Frames
  AVFrame *cpuBgrFrame{};     // Pointers for PipeWire memory
  AVFrame *filteredHwFrame{}; // Filter result (NV12 on VRAM)

  static void printAVError(const char *message);
  static void printAVError(int &ret, const char *message);
  void cleanup();
};