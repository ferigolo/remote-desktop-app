#pragma once
#include <rtc/rtc.hpp>
#include <memory>
#include <string>
#include <print>

enum class SignalingMessageType
{
  Register,
  Offer,
  Answer,
  Candidate,
  RequestOffer
};

class WebRtcManager
{
private:
  // libdatachannel's built-in WebSocket client
  std::shared_ptr<rtc::WebSocket> ws;
  std::shared_ptr<rtc::PeerConnection> pc;
  std::shared_ptr<rtc::DataChannel> dataChannel;
  std::shared_ptr<rtc::Track> videoTrack;
  std::shared_ptr<rtc::RtpPacketizationConfig> rtpConfig;
  uint32_t videoFps = 60;

  constexpr SignalingMessageType
  parseMessageType(const std::string &typeStr);

public:
  WebRtcManager();
  ~WebRtcManager();

  void setVideoFps(uint32_t fps);

  // Connects to the Python Signaling Server
  void connectSignaling(const std::string &url);
  void initializePeerConnection();
  void initializeDataChannel();
  void initializeVideoTrack();
  void sendVideoPacket(const uint8_t *data, size_t size);
};