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
                                       nullptr, 0);
      ret < 0)
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

  if (int ret = av_hwframe_ctx_init(hwFramesRef); ret < 0)
    return printAvErrorAndReturn(ret, "Failed initializing frames context");

  codecCtx->hw_frames_ctx = av_buffer_ref(hwFramesRef);

  // Now encoder knows where its hardware memory pool is, we can open it
  if (int ret = avcodec_open2(codecCtx, codec, nullptr); ret < 0)
    return printAvErrorAndReturn(ret, "Failed to open the encoder");

  isInitialized = true;
  return true;
}

AVFrame* IntelIrisEncoder::createDrmFrame(int fd, int width, int height,
                                          int stride, uint64_t modifier,
                                          uint32_t spaFormat) {
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

void IntelIrisEncoder::encode(int fd, int width, int height, int stride,
                              uint64_t modifier, uint32_t spaFormat) {
  // Wrap the DMABUF in a DRM hardware frame
  AVFrame* drmFrame =
      createDrmFrame(fd, width, height, stride, modifier, spaFormat);
  // Map DRM -> QSV (Zero-Copy)
  AVFrame* frame = av_frame_alloc();
  frame->format = AV_PIX_FMT_QSV;
  frame->hw_frames_ctx = av_buffer_ref(hwFramesRef);

  // Maps the DMA Buffer straight into QSV
  if (int ret = av_hwframe_map(frame, drmFrame, AV_HWFRAME_MAP_READ); ret < 0) {
    printAvErrorAndReturn(ret, "Failed to map DRM frame to QSV");
    av_frame_free(&drmFrame);
    av_frame_free(&frame);
    return;
  }

  processPacket(frame);  // Send to QSV encoder
  av_frame_free(&drmFrame);
  av_frame_free(&frame);
}

void IntelIrisEncoder::encode(int fd, int width, int height, int stride,
                              uint32_t spaFormat) {}

void IntelIrisEncoder::cleanup() {
  flush();
  if (hwFramesRef) av_buffer_unref(&hwFramesRef);
  if (hwCtx) av_buffer_unref(&hwCtx);
  if (codecCtx) {
    avcodec_free_context(&codecCtx);
  }
  framesCtx = nullptr;
}
