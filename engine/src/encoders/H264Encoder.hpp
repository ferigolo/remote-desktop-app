#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <print>
#include <sys/mman.h>
#include <functional>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/pixdesc.h>
#include <libdrm/drm_fourcc.h>
}

class H264Encoder
{
private:
  AVBufferRef *cuda_device_ctx = nullptr;
  AVCodecContext *enc_ctx = nullptr;
  int64_t pts_counter = 0;

  // Internal helper to pull encoded packets from the encoder
  void receiveAndProcessPackets();
  bool printAVErrorAndReturn(const char *message)
  {
    std::println(std::cerr, "[Encoder] {}", message);
    return false;
  }

protected:
  // Virtual method to be overridden by the user.
  // This is where you get the final H.264 NAL units.
  virtual void onEncodedPacket(AVPacket *pkt);

public:
  H264Encoder(std::function<void(AVPacket *pkt)> onEncodedPacketCallback) : onEncodedPacketCallback(onEncodedPacketCallback) {}
  H264Encoder() = default;
  virtual ~H264Encoder() { cleanup(); }

  std::function<void(AVPacket *pkt)> onEncodedPacketCallback;

  // Initialize the NVENC hardware encoder
  bool initialize(int width, int height, int fps = 60, int bitrate = 5000000);

  // Main function to be called from the PipeWire process callback
  void encodeDmaBuf(int fd, int width, int height, int stride, uint64_t modifier);
  void encodeMemFd(int fd, int width, int height, int stride);

  // Flush the encoder (Call this before shutting down to get remaining delayed frames)
  void flush();
  void cleanup(); // Safely free FFmpeg contexts

  bool isInitialized = false;
};
