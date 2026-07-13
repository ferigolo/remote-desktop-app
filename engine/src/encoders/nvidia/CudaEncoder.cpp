#include "CudaEncoder.hpp"

CudaEncoder::CudaEncoder(
    std::function<void(AVPacket* pkt)> onEncodedPacketCallback)
    : BaseEncoder(onEncodedPacketCallback) {}

CudaEncoder::~CudaEncoder() {
  if (codecCtx) {
    std::println("[CudaEncoder] Flushing remaining frames...");
    flush();
  }
  cleanup();
}

bool CudaEncoder::initialize(int width, int height, int fps, int bitrate) {
  if (av_hwdevice_ctx_create(&deviceCtx, AV_HWDEVICE_TYPE_CUDA, nullptr,
                             nullptr, 0) < 0)
    return printAvErrorAndReturn("Failed to create CUDA device context");

  const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
  codecCtx = avcodec_alloc_context3(codec);
  codecCtx->width = width;
  codecCtx->height = height;
  codecCtx->time_base = {1, fps};
  codecCtx->framerate = {fps, 1};
  codecCtx->pix_fmt = AV_PIX_FMT_CUDA;

  // WebRTC specific configuration for Low Latency and avoiding UDP drop
  codecCtx->bit_rate =
      5000000;  // 5 Mbps (50 Mbps was too high and caused UDP drops)
  codecCtx->max_b_frames =
      0;  // WebRTC hates B-frames (causes "missing during reorder" error)
  codecCtx->gop_size = fps * 2;  // Keyframe every 2 seconds

  // NVENC specific optimizations for Ultra Low Latency
  av_opt_set(codecCtx->priv_data, "preset", "p1", 0);
  av_opt_set(codecCtx->priv_data, "tune", "ull", 0);
  av_opt_set(codecCtx->priv_data, "delay", "0", 0);

  // Link CUDA device context to the encoder
  codecCtx->hw_frames_ctx = av_hwframe_ctx_alloc(deviceCtx);
  AVHWFramesContext* hwFramesCtx =
      (AVHWFramesContext*)codecCtx->hw_frames_ctx->data;
  hwFramesCtx->format = AV_PIX_FMT_CUDA;
  hwFramesCtx->sw_format = AV_PIX_FMT_NV12;  // NVENC prefers NV12
  hwFramesCtx->width = width;
  hwFramesCtx->height = height;

  av_hwframe_ctx_init(codecCtx->hw_frames_ctx);
  avcodec_open2(codecCtx, codec, nullptr);

  isInitialized = true;
  return true;
}

AVFrame* CudaEncoder::createDrmFrame(int fd, int width, int height, int stride,
                                     uint64_t modifier, uint32_t spaFormat) {
  AVFrame* drmFrame = av_frame_alloc();  // DRM -> Direct Rendering Manager
  drmFrame->width = width;
  drmFrame->height = height;
  drmFrame->format = AV_PIX_FMT_DRM_PRIME;
  // This format tells FFmpeg there are no pixels at
  // RAM. Instead, it should look at the DMA-BUF
  // which points to where the image is stored at
  // VRAM (for that, it uses the file descriptor)
  // drmFrame now only holds a desc that holds:
  // - the Pipewire File descriptor (points to VRAM image address)
  // - the resolution
  // - the DRM format
  // - the modifier
  AVDRMFrameDescriptor* desc =
      (AVDRMFrameDescriptor*)av_mallocz(sizeof(AVDRMFrameDescriptor));
  desc->nb_objects = 1;
  auto& obj = desc->objects[0];
  obj.fd = fd;
  obj.size = height * stride;
  obj.format_modifier = modifier;  // How the GPU organized the pixels fisicly

  desc->nb_layers = 1;
  auto& layer = desc->layers[0];
  layer.format = getDrmFormatFromSpa(spaFormat);  // Dynamically set!
  layer.nb_planes = 1;
  layer.planes[0].object_index = 0;
  layer.planes[0].offset = 0;
  layer.planes[0].pitch = stride;

  drmFrame->data[0] = (uint8_t*)desc;

  drmFrame->buf[0] =
      av_buffer_create((uint8_t*)desc, sizeof(*desc),
                       [](void*, uint8_t* data) { av_free(data); }, nullptr, 0);
  return drmFrame;
}

void CudaEncoder::encode(int fd, int width, int height, int stride,
                         uint64_t modifier, uint32_t spaFormat) {
  // Wrap the DMABUF in a DRM hardware frame
  AVFrame* drmFrame =
      createDrmFrame(fd, width, height, stride, modifier, spaFormat);
  // Map DRM -> CUDA (Zero-Copy)
  AVFrame* cudaFrame = av_frame_alloc();
  cudaFrame->format = AV_PIX_FMT_CUDA;
  cudaFrame->hw_frames_ctx = av_buffer_ref(codecCtx->hw_frames_ctx);

  // Maps the DMA Buffer straight into NVENC
  if (av_hwframe_map(cudaFrame, drmFrame, AV_HWFRAME_MAP_READ) < 0) {
    printAvErrorAndReturn("Failed to map DRM frame to CUDA");
    av_frame_free(&drmFrame);
    av_frame_free(&cudaFrame);
    return;
  }

  processPacket(cudaFrame);  // Send to NVENC
  av_frame_free(&drmFrame);
  av_frame_free(&cudaFrame);
}

// Called when frame type is of Memfd
void CudaEncoder::encode(int fd, int width, int height, int stride,
                         uint32_t spaFormat) {
  size_t size = height * stride;
  void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    std::println(stderr, "❌ [CudaEncoder] Failed to mmap MemFd");
    close(fd);
    return;
  }

  AVPixelFormat srcFmt = AV_PIX_FMT_BGRA;
  if (spaFormat == 5)
    srcFmt = AV_PIX_FMT_RGBA;  // SPA_VIDEO_FORMAT_RGBx
  else if (spaFormat == 9)
    srcFmt = AV_PIX_FMT_BGRA;  // SPA_VIDEO_FORMAT_BGRx

  if (!swsCtx) {
    swsCtx =
        sws_getContext(width, height, srcFmt, width, height, AV_PIX_FMT_NV12,
                       SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    nv12CpuFrame = av_frame_alloc();
    nv12CpuFrame->width = width;
    nv12CpuFrame->height = height;
    nv12CpuFrame->format = AV_PIX_FMT_NV12;
    av_frame_get_buffer(nv12CpuFrame, 32);
  }

  const uint8_t* inData[1] = {(const uint8_t*)data};
  int inLinesize[1] = {stride};

  sws_scale(swsCtx, inData, inLinesize, 0, height, nv12CpuFrame->data,
            nv12CpuFrame->linesize);

  AVFrame* hwFrame = av_frame_alloc();
  if (av_hwframe_get_buffer(codecCtx->hw_frames_ctx, hwFrame, 0) < 0) {
    std::println(stderr, "❌ [CudaEncoder] Failed to allocate hardware frame");
    av_frame_free(&hwFrame);
  } else {
    if (av_hwframe_transfer_data(hwFrame, nv12CpuFrame, 0) < 0)
      std::println(stderr,
                   "❌ [CudaEncoder] Failed to upload NV12 frame to CUDA");
    else
      processPacket(hwFrame);

    av_frame_free(&hwFrame);
  }

  munmap(data, size);
  close(fd);
}

void CudaEncoder::cleanup() {
  if (swsCtx) {
    sws_freeContext(swsCtx);
    swsCtx = nullptr;
  }
  if (deviceCtx) av_buffer_unref(&deviceCtx);
  if (nv12CpuFrame) av_frame_free(&nv12CpuFrame);
  if (codecCtx) {
    avcodec_free_context(&codecCtx);
  }
}
