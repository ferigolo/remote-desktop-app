#include "../BaseDecoder.hpp"

class CudaDecoder : public BaseDecoder {
 public:
  CudaDecoder();
  ~CudaDecoder();
  bool initialize() override;

  void decode(const uint8_t* data, size_t size) override;

 private:
  AVPacket* packet{};
  AVFrame* frame{};
  AVBufferRef* hwDeviceCtx{};

  static AVPixelFormat getHwFormat(AVCodecContext* ctx,
                                   const AVPixelFormat* pixFmts);
};