#pragma once
#include <functional>
#include <memory>
#include <rtc/rtc.hpp>
#include <string>

enum class SignalingMessageType {
  Register,
  Offer,
  Answer,
  Candidate,
  RequestOffer
};

class ClientWebRtcManager {
 public:
  ClientWebRtcManager();
  ~ClientWebRtcManager();

  void connectSignaling(const std::string& url);

  std::function<void(const uint8_t* data, size_t size)> onVideoPacketReceived;
  std::function<void(int width, int height)> onResolutionSetCallback;

 private:
  void initializePeerConnection();

  std::shared_ptr<rtc::PeerConnection> pc;
  std::shared_ptr<rtc::WebSocket> ws;
  std::shared_ptr<rtc::Track> videoTrack;
  std::shared_ptr<rtc::DataChannel> dataChannel;
};
