#include "H264Decoder.hpp"
#include <print>
#include <iostream>

H264Decoder::H264Decoder() {}

H264Decoder::~H264Decoder() {
    if (swsCtx) sws_freeContext(swsCtx);
    if (frame) av_frame_free(&frame);
    if (rgbFrame) av_frame_free(&rgbFrame);
    if (decCtx) avcodec_free_context(&decCtx);
}

bool H264Decoder::initialize() {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::println("H264 decoder not found");
        return false;
    }

    decCtx = avcodec_alloc_context3(codec);
    if (!decCtx) return false;

    // Optional: enable multithreaded decoding
    decCtx->thread_count = 4;
    decCtx->thread_type = FF_THREAD_SLICE;

    if (avcodec_open2(decCtx, codec, nullptr) < 0) {
        std::println("Failed to open H264 decoder");
        return false;
    }

    frame = av_frame_alloc();
    rgbFrame = av_frame_alloc();
    isInitialized = true;
    
    // Omits "PPS 0 referenced" FFmpeg logs until get the IDR frame.
    av_log_set_level(AV_LOG_QUIET);
    
    std::println("✅ [Decoder] H264 Decoder Initialized");
    return true;
}

void H264Decoder::decode(const uint8_t* data, size_t size) {
    if (!isInitialized) return;

    AVPacket* pkt = av_packet_alloc();
    pkt->data = (uint8_t*)data;
    pkt->size = size;

    int ret = avcodec_send_packet(decCtx, pkt);
    if (ret < 0) {
        if (hasDecodedFirstFrame) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::println("❌ [Decoder] avcodec_send_packet failed: {}", errbuf);
        }
    } else {
        while (true) {
            ret = avcodec_receive_frame(decCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                if (hasDecodedFirstFrame) {
                    char errbuf[128];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::println("❌ [Decoder] avcodec_receive_frame failed: {}", errbuf);
                }
                break;
            }
            
            if (!hasDecodedFirstFrame) {
                hasDecodedFirstFrame = true;
                av_log_set_level(AV_LOG_WARNING); // Restaura os logs de erro
            }

            // First frame received: initialize sws_scale
            if (!swsCtx) {
                swsCtx = sws_getContext(
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    frame->width, frame->height, AV_PIX_FMT_RGBA,
                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
                );
                
                rgbFrame->format = AV_PIX_FMT_RGBA;
                rgbFrame->width = frame->width;
                rgbFrame->height = frame->height;
                av_image_alloc(rgbFrame->data, rgbFrame->linesize, frame->width, frame->height, AV_PIX_FMT_RGBA, 32);
            }

            sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, rgbFrame->data, rgbFrame->linesize);

            static int frameCount = 0;
            if (frameCount++ % 60 == 0) {
                std::println("✅ [Decoder] Successfully decoded and converted frame #{} ({}x{})", frameCount, frame->width, frame->height);
            }

            if (onFrameDecoded)
                onFrameDecoded(rgbFrame->data[0], rgbFrame->width, rgbFrame->height, rgbFrame->linesize[0]);
        }
    }
    av_packet_free(&pkt);
}
