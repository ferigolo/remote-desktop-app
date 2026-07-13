#include "WebRtcManager.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

WebRtcManager::WebRtcManager() { rtc::InitLogger(rtc::LogLevel::Info); }

WebRtcManager::~WebRtcManager() {
  {
    std::lock_guard<std::mutex> lock(trackMutex);
    stopWaiting = true;
    trackCv.notify_all();
  }
  if (dataChannel) dataChannel->close();
  if (pc) pc->close();
  if (ws) ws->close();
}

constexpr SignalingMessageType WebRtcManager::parseMessageType(
    const std::string& typeStr) {
  if (typeStr == "register") return SignalingMessageType::Register;
  if (typeStr == "offer") return SignalingMessageType::Offer;
  if (typeStr == "answer") return SignalingMessageType::Answer;
  return SignalingMessageType::Candidate;
}

void WebRtcManager::initializePeerConnection() {
  rtc::Configuration config;
  config.iceServers.emplace_back("stun:stun.l.google.com:19302");
  config.disableAutoNegotiation = true;

  pc = std::make_shared<rtc::PeerConnection>(config);
  // Setup ICE Candidate ghatering
  pc->onLocalCandidate([this](rtc::Candidate candidate) {
    std::println(
        "🧊 [WebRtcManager] Found ICE Candidate, sending to signaling "
        "server...");
    json msg;
    msg["type"] = SignalingMessageType::Candidate;
    msg["target"] = "client";  // Send to the web browser
    msg["candidate"] = candidate.candidate();
    msg["sdpMid"] = candidate.mid();

    ws->send(msg.dump());
  });

  pc->onLocalDescription([this](rtc::Description description) {
    json msg;
    msg["type"] = (description.type() == rtc::Description::Type::Offer)
                      ? SignalingMessageType::Offer
                      : SignalingMessageType::Answer;
    msg["target"] = "client";
    msg["sdp"] = std::string(description);
    ws->send(msg.dump());
  });
}

void WebRtcManager::initializeDataChannel() {
  if (!pc) throw std::runtime_error("Peer connection not initialized");
  dataChannel = pc->createDataChannel("input");

  dataChannel->onOpen(
      []() { std::println("✅ [WebRtcManager] Input DataChannel Opened!"); });

  dataChannel->onMessage([](std::variant<rtc::binary, rtc::string> data) {
    if (std::holds_alternative<rtc::binary>(data)) {
      // TODO: receive binary packets from client and pass them than through
      // Linux /dev/uinput
      std::println("🖱️ [WebRtcManager] Received binary input packet!");
    } else if (std::holds_alternative<rtc::string>(data)) {
      std::string text = std::get<rtc::string>(data);
      std::println("📝 [WebRtcManager] Received text on DataChannel: {}", text);
    }
  });
}

void WebRtcManager::initializeVideoTrack() {
  if (!dataChannel) throw std::runtime_error("Data channel not initialized");

  // Identificadores de mídia que precisam bater entre as duas pontas
  const uint8_t payloadType = 96;
  const uint32_t ssrc = 42;
  const std::string cname = "video-stream";

  rtc::Description::Video videoMedia("video",
                                     rtc::Description::Direction::SendOnly);
  videoMedia.addH264Codec(payloadType);
  videoMedia.addSSRC(ssrc, cname);
  videoTrack = pc->addTrack(videoMedia);

  videoTrack->onOpen([this]() {
    std::println("✅ [WebRtcManager] Video Track Opened!");
    std::lock_guard<std::mutex> lock(trackMutex);
    isVideoTrackOpen = true;
    trackCv.notify_all();
  });

  videoTrack->onClosed([this]() {
    std::println("❌ [WebRtcManager] Video Track Closed!");
    std::lock_guard<std::mutex> lock(trackMutex);
    isVideoTrackOpen = false;
    trackCv.notify_all();
  });

  this->rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
      ssrc, cname, payloadType, rtc::H264RtpPacketizer::ClockRate);

  // 2. Avisamos a biblioteca que a NVIDIA usa StartSequence (00 00 00 01) e
  // fatiamos os frames
  auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(
      rtc::NalUnit::Separator::StartSequence, this->rtpConfig);

  // Reporting packet loss to the client
  auto srReporter = std::make_shared<rtc::RtcpSrReporter>(this->rtpConfig);
  packetizer->addToChain(srReporter);
  auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
  packetizer->addToChain(nackResponder);

  videoTrack->setMediaHandler(packetizer);
}

void WebRtcManager::setVideoFps(uint32_t fps) {
  this->videoFps = (fps > 0) ? fps : 60;
}

void WebRtcManager::sendVideoPacket(const uint8_t* data, size_t size) {
  {
    std::unique_lock<std::mutex> lock(trackMutex);
    if (!isVideoTrackOpen) {
      std::println("⏳ [WebRtcManager] videoTrack not open yet, waiting...");
      trackCv.wait(lock, [this]() { return isVideoTrackOpen || stopWaiting; });
    }
  }

  if (stopWaiting || !videoTrack || !videoTrack->isOpen()) return;

  if (this->rtpConfig) this->rtpConfig->timestamp += (90000 / this->videoFps);

  rtc::binary packet_data(reinterpret_cast<const std::byte*>(data),
                          reinterpret_cast<const std::byte*>(data) + size);
  if (!videoTrack->send(packet_data))
    std::println(
        "❌ [WebRtcManager] Failed to send packet data (track->send returned "
        "false)");
}

void WebRtcManager::sendResolution(const int width, const int height) {
  if (!(dataChannel && dataChannel->isOpen())) return;
  json msg;
  msg["type"] = "resolution";
  msg["width"] = width;
  msg["height"] = height;
  dataChannel->send(msg.dump());
}

void WebRtcManager::connectSignaling(const std::string& url) {
  ws = std::make_shared<rtc::WebSocket>();

  ws->onOpen([this]() {
    std::println("🌐 [WebRtcManager] Connected to Signaling Server!");
    // Send our registration JSON
    json regMsg;
    regMsg["type"] = SignalingMessageType::Register;
    regMsg["id"] = "host";
    ws->send(regMsg.dump());
  });

  // Listen for incoming SDP / ICE messages from the Client
  ws->onMessage([this](std::variant<rtc::binary, rtc::string> data) {
    if (!std::holds_alternative<rtc::string>(data)) return;

    std::string msg = std::get<rtc::string>(data);
    // std::println("📩 [WebRtcManager] Received message: {}", msg);

    auto parsedJson = json::parse(msg);
    SignalingMessageType type = parsedJson["type"].get<SignalingMessageType>();
    switch (type) {
      case SignalingMessageType::Candidate: {
        std::println("🧊 [WebRtcManager] Received ICE Candidate from Client");
        std::string candString = parsedJson["candidate"].get<std::string>();
        std::string midString = parsedJson["sdpMid"].get<std::string>();
        rtc::Candidate candidate(candString, midString);
        pc->addRemoteCandidate(candidate);
        break;
      }
      case SignalingMessageType::RequestOffer: {
        std::println(
            "🤝 [WebRtcManager] Client requested connection! Generating "
            "Offer...");
        pc->setLocalDescription();
        break;
      }
      case SignalingMessageType::Answer: {
        std::println("🤝 [WebRtcManager] Received Answer from Client!");
        std::string sdpString = parsedJson["sdp"].get<std::string>();
        rtc::Description desc(sdpString, "answer");
        pc->setRemoteDescription(desc);
        break;
      }

      default:
        std::println("⚠️ [WebRtcManager] Unknown message type");
        break;
    }
  });

  ws->onClosed([]() {
    std::println("❌ [WebRtcManager] Disconnected from Signaling Server.");
  });

  ws->onError([](const std::string& err) {
    std::println("❌ [WebRtcManager] WebSocket Error: {}", err);
  });

  // Actually open the connection
  std::println("🌐 [WebRtcManager] Connecting to {}...", url);
  ws->open(url);
}
