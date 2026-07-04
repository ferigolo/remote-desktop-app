#include "H264Encoder.hpp"

H264Encoder::H264Encoder(int width, int height, int fps) : width(width), height(height), fps(fps) {}

H264Encoder::~H264Encoder()
{
  cleanup();
}

bool H264Encoder::initialize()
{
  const AVCodec *codec = avcodec_find_encoder_by_name("h264_vaapi");
  if (!codec)
  {
    printAVError("Codec h264_vaapi not found. Missing hardware drivers on Linux");
    return false;
  }

  if (int ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0)
  {
    printAVError(ret, "GPU access failed");
    return false;
  }

  // Basic config
  codecContext = avcodec_alloc_context3(codec);
  if (!codecContext)
    return false;
  codecContext->width = width;
  codecContext->height = height;
  codecContext->time_base = {1, fps};
  codecContext->framerate = {fps, 1};
  codecContext->pix_fmt = AV_PIX_FMT_VAAPI; // Frames will come directly from VRAM
  codecContext->bit_rate = 5000000;

  av_opt_set(codecContext->priv_data, "profile", "baseline", 0);
  av_opt_set(codecContext->priv_data, "rc_mode", "CBR", 0);
  filterGraph = avfilter_graph_alloc();

  char args[512];
  std::snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",
                width, height, AV_PIX_FMT_BGR0, 1, fps);

  const AVFilter *buffersrc = avfilter_get_by_name("buffer");
  const AVFilter *buffersink = avfilter_get_by_name("buffersink");

  avfilter_graph_create_filter(&bufSrcCtx, buffersrc, "in", args, nullptr, filterGraph);
  bufSrcCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
  avfilter_graph_create_filter(&bufSinkCtx, buffersink, "out", nullptr, nullptr, filterGraph);

  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();
  outputs->name = av_strdup("in");
  outputs->filter_ctx = bufSrcCtx;
  outputs->pad_idx = 0;
  outputs->next = nullptr;
  inputs->name = av_strdup("out");
  inputs->filter_ctx = bufSinkCtx;
  inputs->pad_idx = 0;
  inputs->next = nullptr;

  // Converts BGR0 into NV12
  if (avfilter_graph_parse_ptr(filterGraph, "hwupload,scale_vaapi=format=nv12", &inputs, &outputs, nullptr) < 0 ||
      avfilter_graph_config(filterGraph, nullptr) < 0)
  {
    printAVError("Pipeline configuration of GPU conversion failed");
    return false;
  }
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  // Gets filter results to feed encoder
  codecContext->hw_frames_ctx = av_buffer_ref(av_buffersink_get_hw_frames_ctx(bufSinkCtx));

  if (int ret = avcodec_open2(codecContext, codec, nullptr) < 0)
  {
    printAVError(ret, "Failed to open VAAPI encoder");
    return false;
  }

  cpuBgrFrame = av_frame_alloc();
  cpuBgrFrame->format = AV_PIX_FMT_BGR0;
  cpuBgrFrame->width = width;
  cpuBgrFrame->height = height;

  filteredHwFrame = av_frame_alloc();
  packet = av_packet_alloc();

  std::println("✅ [Encoder] GPU VPP Pipeline (BGR0 -> NV12 -> H.264) initialized: {}x{} @ {}fps", width, height, fps);
  return true;
}

bool H264Encoder::encode(const VideoFrame &frame, std::function<void(const uint8_t *data, size_t size)> on_packet_ready)
{
  // Zero CPU copy
  cpuBgrFrame->data[0] = frame.data;
  cpuBgrFrame->linesize[0] = frame.stride;
  cpuBgrFrame->pts = frameCounter++;

  if (int ret = av_buffersrc_add_frame_flags(bufSrcCtx, cpuBgrFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) // Keep a reference to the frame
  {
    printAVError(ret, "Failed sending frame to GPU");
    return false;
  }

  while (av_buffersink_get_frame(bufSinkCtx, filteredHwFrame) >= 0)
  {
    // Sends to compression
    if (int ret = avcodec_send_frame(codecContext, filteredHwFrame) < 0)
    {
      printAVError(ret, "Failed to send frame for compression");
      return false;
    }

    while (avcodec_receive_packet(codecContext, packet) >= 0)
    {
      if (on_packet_ready)
        on_packet_ready(packet->data, packet->size);
      av_packet_unref(packet);
    }

    av_frame_unref(filteredHwFrame); // Free VRAM reference
  }

  return true;
}

void H264Encoder::printAVError(const char *message)
{
  std::println(stderr, "❌ [Encoder H264] {}", message);
}

void H264Encoder::printAVError(int &ret, const char *message)
{
  static char errbuf[AV_ERROR_MAX_STRING_SIZE];
  av_strerror(ret, errbuf, sizeof(errbuf));

  std::println(stderr, "❌ [Encoder H264] {} (VAAPI). Error: {} (Code: {})", message, errbuf, ret);
}

void H264Encoder::cleanup()
{
  if (filterGraph)
    avfilter_graph_free(&filterGraph);
  if (cpuBgrFrame)
    av_frame_free(&cpuBgrFrame);
  if (filteredHwFrame)
    av_frame_free(&filteredHwFrame);
  if (packet)
    av_packet_free(&packet);
  if (codecContext)
    avcodec_free_context(&codecContext);
  if (hwDeviceCtx)
    av_buffer_unref(&hwDeviceCtx);
};