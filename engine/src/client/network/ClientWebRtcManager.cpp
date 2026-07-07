#include "ClientWebRtcManager.hpp"

#include <iostream>
#include <nlohmann/json.hpp>
#include <print>

using json = nlohmann::json;

ClientWebRtcManager::ClientWebRtcManager() {}

ClientWebRtcManager::~ClientWebRtcManager() {
  if (pc) pc->close();
  if (ws) ws->close();
}

class CustomRtpLogger : public rtc::MediaHandler {
 public:
  CustomRtpLogger() = default;
  ~CustomRtpLogger() override = default;

  void incoming(rtc::message_vector& messages,
                const rtc::message_callback&) override {
    for (auto& msg : messages) {
      static int count = 0;
      if (count++ % 60 == 0) {
        std::println("📦 [CustomRtpLogger] Received raw RTP packet of size {}",
                     msg->size());
      }
    }
  }
};

void ClientWebRtcManager::initializePeerConnection() {
  rtc::Configuration config;
  config.iceServers.emplace_back("stun:stun.l.google.com:19302");
  config.disableAutoNegotiation = true;

  pc = std::make_shared<rtc::PeerConnection>(config);

  pc->onLocalCandidate([this](rtc::Candidate candidate) {
    json msg;
    msg["type"] = SignalingMessageType::Candidate;
    msg["target"] = "host";
    msg["candidate"] = candidate.candidate();
    msg["sdpMid"] = candidate.mid();
    ws->send(msg.dump());
  });

  pc->onLocalDescription([this](rtc::Description description) {
    json msg;
    msg["type"] = (description.type() == rtc::Description::Type::Offer)
                      ? SignalingMessageType::Offer
                      : SignalingMessageType::Answer;
    msg["sdp"] = std::string(description);
    msg["target"] = "host";
    ws->send(msg.dump());
  });

  pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
    std::println("🎬 [ClientWebRtcManager] Received remote track!");
    this->videoTrack = track;  // Store the remote track to keep it alive

    track->onOpen([track]() {
      std::println("🎬 [ClientWebRtcManager] Remote track opened!");
      track->requestKeyframe();
    });

    auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>(
        rtc::NalUnit::Separator::StartSequence);
    track->setMediaHandler(depacketizer);

    track->onFrame([this](rtc::binary bin, rtc::FrameInfo frame) {
      static int pktCount = 0;
      if (pktCount++ % 60 == 0) {
        std::println(
            "📦 [ClientWebRtcManager] Received video frame of size {} (Total "
            "frames: {})",
            bin.size(), pktCount);
      }

      if (onVideoPacketReceived) {
        onVideoPacketReceived(reinterpret_cast<const uint8_t*>(bin.data()),
                              bin.size());
      }
    });
  });
}

void ClientWebRtcManager::connectSignaling(const std::string& url) {
  ws = std::make_shared<rtc::WebSocket>();

  ws->onOpen([this]() {
    std::println("🌐 [ClientWebRtcManager] Connected to Signaling Server!");

    json regMsg;
    regMsg["type"] = SignalingMessageType::Register;
    regMsg["id"] = "client";
    ws->send(regMsg.dump());

    initializePeerConnection();

    json reqMsg;
    reqMsg["type"] = SignalingMessageType::RequestOffer;
    reqMsg["target"] = "host";
    ws->send(reqMsg.dump());
  });

  ws->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
    if (!std::holds_alternative<rtc::string>(data)) return;

    auto parsedJson = json::parse(std::get<rtc::string>(data));
    SignalingMessageType type = parsedJson["type"].get<SignalingMessageType>();

    if (type == SignalingMessageType::Offer) {
      std::println("🤝 [ClientWebRtcManager] Received Offer!");
      rtc::Description desc(parsedJson["sdp"].get<std::string>(), "offer");
      pc->setRemoteDescription(desc);
      pc->setLocalDescription();  // This generates the Answer and triggers
                                  // onLocalDescription!
    } else if (type == SignalingMessageType::Candidate) {
      rtc::Candidate candidate(parsedJson["candidate"].get<std::string>(),
                               parsedJson["sdpMid"].get<std::string>());
      pc->addRemoteCandidate(candidate);
    }
  });

  ws->open(url);
}
