#include "../BaseEncoder.hpp"

extern "C" {
#include <libavutil/hwcontext_qsv.h>
}

class IntelIrisEncoder : public BaseEncoder {
 public:
  IntelIrisEncoder() = default;
  IntelIrisEncoder(std::function<void(AVPacket* pkt)> onEncodedPacketCallback);
  ~IntelIrisEncoder();

  std::function<void(AVPacket* pkt)> onEncodedPacketCallback;

  bool initialize(int width, int height, int fps = 60,
                  int bitrate = 50000000) override;
  void encode(int fd, int width, int height, int stride, uint64_t modifier,
              uint32_t spaFormat) override;
  void encode(int fd, int width, int height, int stride,
              uint32_t spaFormat) override;

 private:
  AVBufferRef* hwCtx{};
  AVBufferRef* hwFramesRef{};
  AVHWFramesContext* framesCtx{};

  AVFrame* createDrmFrame(int fd, int width, int height, int stride,
                          uint64_t modifier, uint32_t spaFormat);
                          
  void cleanup() override;
};