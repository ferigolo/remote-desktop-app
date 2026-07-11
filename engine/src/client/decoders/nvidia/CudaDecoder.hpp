#include "../BaseDecoder.hpp"

class CudaDecoder : public BaseDecoder {
 public:
  CudaDecoder();
  ~CudaDecoder();
  bool initialize() override;

  void decode(const uint8_t* data, size_t size) override;
  void updateTexture(SDL_Renderer* renderer, SDL_Texture** texture) override;

 private:
  AVFrame* frame{};
  AVBufferRef* hwDeviceCtx{};

  static AVPixelFormat getHwFormat(AVCodecContext* ctx,
                                   const AVPixelFormat* pixFmts);
};