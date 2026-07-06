#pragma once
#include <cstdint>
#include <functional>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class H264Decoder {
public:
    H264Decoder();
    ~H264Decoder();

    bool initialize();
    void decode(const uint8_t* data, size_t size);

    std::function<void(const uint8_t* rgbaData, int width, int height, int pitch)> onFrameDecoded;

private:
    AVCodecContext* decCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbFrame = nullptr;
    SwsContext* swsCtx = nullptr;

    bool isInitialized = false;
    bool hasDecodedFirstFrame = false;
};
