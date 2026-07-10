#include "../BaseEncoder.hpp"

extern "C" {
#include <libavutil/hwcontext_cuda.h>
#include <libswscale/swscale.h>
}

class CudaEncoder : public BaseEncoder {
 public:
  CudaEncoder() = default;
  CudaEncoder(std::function<void(AVPacket* pkt)> onEncodedPacketCallback);
  ~CudaEncoder();

  bool initialize(int width, int height, int fps = 60,
                  int bitrate = 50000000) override;

 private:
  AVBufferRef* deviceCtx;
  SwsContext* swsCtx = nullptr;
  AVFrame* nv12CpuFrame = nullptr;

  AVFrame* createDrmFrame(int fd, int width, int height, int stride,
                          uint64_t modifier, uint32_t spaFormat);
  void encode(int fd, int width, int height, int stride, uint64_t modifier,
              uint32_t spaFormat) override;
  void encode(int fd, int width, int height, int stride,
              uint32_t spaFormat) override;

  void flush() override;
  void cleanup() override;
};