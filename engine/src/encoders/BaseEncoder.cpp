#include "BaseEncoder.hpp"

BaseEncoder::BaseEncoder(
    std::function<void(AVPacket* pkt)> onEncodedPacketCallback)
    : onEncodedPacketCallback(std::move(onEncodedPacketCallback)) {}

void BaseEncoder::processPacket(const AVFrame* frame) {
  avcodec_send_frame(codecCtx, frame);

  AVPacket* pkt = av_packet_alloc();
  int ret;
  while ((ret = avcodec_receive_packet(codecCtx, pkt)) >= 0) {
    if (onEncodedPacketCallback) onEncodedPacketCallback(pkt);
    av_packet_unref(pkt);
  }
  av_packet_free(&pkt);
}

void BaseEncoder::flush() {
  if (!codecCtx) return;
  std::println("[Encoder] Flushing remaining frames...");
  avcodec_send_frame(codecCtx, nullptr);  // Signals end of the stream

  AVPacket* pkt = av_packet_alloc();
  int ret;
  // Safely loop only while we receive valid packets (ret == 0)
  while ((ret = avcodec_receive_packet(codecCtx, pkt)) >= 0) {
    if (onEncodedPacketCallback) onEncodedPacketCallback(pkt);
    av_packet_unref(pkt);
  }

  if (ret != AVERROR_EOF)
    printAvErrorAndReturn(
        ret, "Flush ended with an unexpected error rather than EOF");

  av_packet_free(&pkt);
}

bool BaseEncoder::printAvErrorAndReturn(std::string_view contextMessage) {
  std::println(std::cerr, "❌ [Decoder] {} ", contextMessage);
  return false;
}

bool BaseEncoder::printAvErrorAndReturn(int ret,
                                        std::string_view contextMessage) {
  if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    // Translate the error code to a readable string
    if (av_strerror(ret, errbuf, sizeof(errbuf)) < 0)
      snprintf(errbuf, sizeof(errbuf), "Unknown error");

    std::println(std::cerr, "❌ [Decoder] {} | FFmpeg error: {} (Code: {})",
                 contextMessage, errbuf, ret);
  }
  return false;
}
