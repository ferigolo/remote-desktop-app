#pragma once
#include <functional>

#include "../BaseDecoder.hpp"

class SoftwareDecoder : public BaseDecoder {
 public:
  SoftwareDecoder();
  ~SoftwareDecoder();

  bool initialize() override;
  void decode(const uint8_t* data, size_t size) override;

 private:
  AVFrame* frame{};
  AVFrame* swFrame{};
  SwsContext* swsCtx{};
};
