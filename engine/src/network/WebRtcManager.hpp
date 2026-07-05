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
  Unknown
};

class WebRtcManager
{
private:
  // libdatachannel's built-in WebSocket client
  std::shared_ptr<rtc::WebSocket> ws;
  std::shared_ptr<rtc::PeerConnection> pc;
  std::shared_ptr<rtc::DataChannel> dataChannel;
  std::shared_ptr<rtc::Track> videoTrack;

  constexpr SignalingMessageType
  parseMessageType(const std::string &typeStr);

public:
  WebRtcManager();
  ~WebRtcManager();

  // Connects to the Python Signaling Server
  void connectSignaling(const std::string &url);
  void initializePeerConnection();
  void initializeDataChannel();
  void initializeVideoTrack();
  void sendVideoPacket(const uint8_t *data, size_t size);
};