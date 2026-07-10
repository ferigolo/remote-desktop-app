// Uses Intel QSV (Quick Sync Video)
// Setup: initializes a hardware context and transfer the decoded frames from
// the GPU back to the CPU before rendering

#include "../BaseDecoder.hpp"

extern "C" {
#include <libavutil/hwcontext.h>  // REQUIRED FOR HW DECODING
}

class IntelIrisDecoder : public BaseDecoder {
 public:
  IntelIrisDecoder();
  ~IntelIrisDecoder();

  bool initialize() override;
  void decode(const uint8_t* data, size_t size) override;

 private:
  AVCodecContext* codecCtx;
  AVBufferRef* deviceCtx;
  
  AVFrame* hwFrame;
  AVFrame* swFrame;
};
