#include "H264Encoder.hpp"

void H264Encoder::receiveAndProcessPackets()
{
  AVPacket *pkt = av_packet_alloc();
  if (!pkt)
  {
    printAVErrorAndReturn("Failed to allocate AVPacket");
    return;
  }

  while (true)
  {
    int ret = avcodec_receive_packet(encCtx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break; // No more packets available at this moment
    else if (ret < 0)
    {
      printAVErrorAndReturn("Error receiving packet from encoder");
      break;
    }

    // Route the encoded packet to our virtual handler
    onEncodedPacket(pkt);
    av_packet_unref(pkt);
  }
  av_packet_free(&pkt);
}

void H264Encoder::onEncodedPacket(AVPacket *pkt)
{
  static int packetCount = 0;
  packetCount++;

  if (packetCount % 60 == 0)
    std::println("[Encoder] Generated H.264 Packet #{} - Size: {} bytes, PTS: {}", packetCount, pkt->size, pkt->pts);

  onEncodedPacketCallback(pkt);
  // Example: Here you would write pkt->data to an MP4 file,
  // or send it over a WebRTC/RTSP socket.
}

bool H264Encoder::initialize(int width, int height, int fps, int bitrate)
{
  this->fps = fps;
  this->start_time = std::chrono::steady_clock::now();

  std::println("[Encoder] Initializing NVIDIA Hardware Encoder...");

  // 1. Create the CUDA hardware device context
  if (int ret = av_hwdevice_ctx_create(&cuda_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0)
    return printAVErrorAndReturn("Failed to create CUDA hardware context");

  // 2. Find the NVIDIA H.264 encoder
  const AVCodec *codec = avcodec_find_encoder_by_name("h264_nvenc");
  if (!codec)
    return printAVErrorAndReturn("h264_nvenc codec not found! Ensure NVIDIA drivers and FFmpeg non-free are installed");

  encCtx = avcodec_alloc_context3(codec);
  if (!encCtx)
    return printAVErrorAndReturn("Failed to allocate codec context");

  // 3. Bind the hardware context to the encoder
  encCtx->hw_device_ctx = av_buffer_ref(cuda_device_ctx);
  encCtx->pix_fmt = AV_PIX_FMT_CUDA; // Crucial: Encoder expects native CUDA frames

  AVBufferRef *hw_frames_ref = av_hwframe_ctx_alloc(cuda_device_ctx);
  if (!hw_frames_ref)
    return printAVErrorAndReturn("Failed to allocate hw frames context");

  AVHWFramesContext *framesCtx = (AVHWFramesContext *)hw_frames_ref->data;
  framesCtx->format = AV_PIX_FMT_CUDA;
  framesCtx->sw_format = AV_PIX_FMT_BGR0; // Corresponds to DRM_FORMAT_XRGB8888
  framesCtx->width = width;
  framesCtx->height = height;
  framesCtx->initial_pool_size = 0;

  if (av_hwframe_ctx_init(hw_frames_ref) < 0)
  {
    av_buffer_unref(&hw_frames_ref);
    return printAVErrorAndReturn("Failed to initialize hw frames context");
  }

  encCtx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
  av_buffer_unref(&hw_frames_ref);

  // 4. Set video parameters
  encCtx->width = width;
  encCtx->height = height;
  encCtx->time_base = av_make_q(1, fps);
  encCtx->framerate = av_make_q(fps, 1);
  encCtx->bit_rate = bitrate;
  encCtx->gop_size = fps * 2; // Keyframe every 2 seconds
  encCtx->profile = FF_PROFILE_H264_BASELINE;
  encCtx->flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;
  
  av_opt_set(encCtx->priv_data, "preset", "p1", 0);
  av_opt_set(encCtx->priv_data, "tune", "ull", 0); // Ultra Low Latency
  av_opt_set(encCtx->priv_data, "zerolatency", "1", 0);
  av_opt_set(encCtx->priv_data, "repeat_sps_pps", "1", 0); // NVENC option
  
  std::println("[Encoder] time_base={}/{} framerate={}/{}", encCtx->time_base.num, encCtx->time_base.den, encCtx->framerate.num, encCtx->framerate.den);

  // 5. Open the encoder
  if (avcodec_open2(encCtx, codec, nullptr) < 0)
    return printAVErrorAndReturn(" Failed to open codec");

  isInitialized = true;
  std::println("[Encoder] Successfully initialized: {}x{}@{}fps", width, height, fps);
  return true;
}

void H264Encoder::encodeDmaBuf(int fd, int width, int height, int stride, uint64_t modifier)
{
  if (!encCtx)
  {
    printAVErrorAndReturn("Encoder not initialized");
    close(fd); // Prevent FD leak
    return;
  }

  // --- STEP 1: Wrap the DMA-BUF in an AVFrame (DRM PRIME format) ---
  AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *)av_mallocz(sizeof(*desc));
  if (!desc)
  {
    close(fd);
    return;
  }

  desc->nb_objects = 1;
  desc->objects[0].fd = fd;
  desc->objects[0].size = height * stride;
  desc->objects[0].format_modifier = modifier;

  desc->nb_layers = 1;
  desc->layers[0].format = DRM_FORMAT_XRGB8888; // Match with what requested from PipeWire
  desc->layers[0].nb_planes = 1;
  desc->layers[0].planes[0].object_index = 0;
  desc->layers[0].planes[0].offset = 0;
  desc->layers[0].planes[0].pitch = stride;

  AVFrame *drm_frame = av_frame_alloc();
  drm_frame->format = AV_PIX_FMT_DRM_PRIME;
  drm_frame->width = width;
  drm_frame->height = height;
  drm_frame->data[0] = (uint8_t *)desc;

  // Set up the custom destructor for the buffer.
  // When drm_frame is freed, FFmpeg will automatically call this lambda.
  drm_frame->buf[0] = av_buffer_create(
      (uint8_t *)desc, sizeof(*desc),
      [](void *opaque, uint8_t *data)
      {
        AVDRMFrameDescriptor *d = (AVDRMFrameDescriptor *)data;
        close(d->objects[0].fd); // Safely close the duplicated FD
        av_free(d);
      },
      nullptr, 0);

  // --- STEP 2: Map the DRM Frame to the NVIDIA (CUDA) Context (ZERO-COPY) ---
  AVFrame *cuda_frame = av_frame_alloc();
  cuda_frame->format = AV_PIX_FMT_CUDA;

  if (int ret = av_hwframe_map(cuda_frame, drm_frame, AV_HWFRAME_MAP_READ) < 0)
  {
    printAVErrorAndReturn("Failed to map DRM frame to CUDA. Modifier issue?");
    av_frame_free(&cuda_frame);
    av_frame_free(&drm_frame);
    return;
  }

  auto now = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
  cuda_frame->pts = (duration * this->fps) / 1000000;

  // --- STEP 3: Send to Hardware Encoder ---
  if (int ret = avcodec_send_frame(encCtx, cuda_frame) < 0)
    printAVErrorAndReturn("Error sending frame to encoder");
  else
    receiveAndProcessPackets();

  // --- STEP 4: Cleanup Frame Memory ---
  // Freeing cuda_frame drops its reference to drm_frame.
  // Freeing drm_frame triggers the buffer destructor lambda, closing the FD.
  av_frame_free(&cuda_frame);
  av_frame_free(&drm_frame);
}

void H264Encoder::flush()
{
  if (!encCtx)
    return;
  std::println("[Encoder] Flushing remaining frames...");
  avcodec_send_frame(encCtx, nullptr);
  receiveAndProcessPackets();
}

void H264Encoder::cleanup()
{
  flush();
  if (encCtx)
  {
    avcodec_free_context(&encCtx);
    encCtx = nullptr;
  }
  if (cuda_device_ctx)
  {
    av_buffer_unref(&cuda_device_ctx);
    cuda_device_ctx = nullptr;
  }
}

void H264Encoder::encodeMemFd(int fd, int width, int height, int stride)
{
  if (!encCtx)
  {
    printAVErrorAndReturn("Encoder not initialized");
    close(fd);
    return;
  }

  size_t size = height * stride;
  void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED)
  {
    printAVErrorAndReturn("Failed to mmap MemFd");
    close(fd);
    return;
  }

  AVFrame *sw_frame = av_frame_alloc();
  sw_frame->format = AV_PIX_FMT_BGR0;
  sw_frame->width = width;
  sw_frame->height = height;
  sw_frame->linesize[0] = stride;
  sw_frame->data[0] = (uint8_t *)data;

  auto now = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
  sw_frame->pts = (duration * this->fps) / 1000000;

  struct MemFdContext
  {
    int fd;
    size_t size;
  };
  MemFdContext *ctx = new MemFdContext{fd, size};

  sw_frame->buf[0] = av_buffer_create(
      (uint8_t *)data, size,
      [](void *opaque, uint8_t *data)
      {
        MemFdContext *ctx = (MemFdContext *)opaque;
        munmap(data, ctx->size);
        close(ctx->fd);
        delete ctx;
      },
      ctx, 0);

  // Allocate a CUDA hardware frame
  AVFrame *hw_frame = av_frame_alloc();
  if (!hw_frame)
  {
    printAVErrorAndReturn("Failed to allocate hw_frame");
    av_frame_free(&sw_frame);
    return;
  }

  if (int ret = av_hwframe_get_buffer(encCtx->hw_frames_ctx, hw_frame, 0) < 0)
  {
    printAVErrorAndReturn("Failed to allocate CUDA buffer for MemFd frame");
    av_frame_free(&hw_frame);
    av_frame_free(&sw_frame);
    return;
  }

  // Upload the frame from CPU (MemFd) to GPU (CUDA)
  if (int ret = av_hwframe_transfer_data(hw_frame, sw_frame, 0) < 0)
  {
    printAVErrorAndReturn("Failed to transfer MemFd data to CUDA GPU");
    av_frame_free(&hw_frame);
    av_frame_free(&sw_frame);
    return;
  }

  hw_frame->pts = sw_frame->pts;

  if (int ret = avcodec_send_frame(encCtx, hw_frame) < 0)
    printAVErrorAndReturn("Error sending hw_frame to encoder");
  else
    receiveAndProcessPackets();

  av_frame_free(&hw_frame);
  av_frame_free(&sw_frame);
}