#include "WebRtcManager.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WebRtcManager::WebRtcManager() { rtc::InitLogger(rtc::LogLevel::Info); }

WebRtcManager::~WebRtcManager()
{
  if (dataChannel)
    dataChannel->close();
  if (pc)
    pc->close();
  if (ws)
    ws->close();
}

constexpr SignalingMessageType WebRtcManager::parseMessageType(const std::string &typeStr)
{
  if (typeStr == "register")
    return SignalingMessageType::Register;
  if (typeStr == "offer")
    return SignalingMessageType::Offer;
  if (typeStr == "answer")
    return SignalingMessageType::Answer;
  if (typeStr == "candidate")
    return SignalingMessageType::Candidate;
  return SignalingMessageType::Unknown;
}

void WebRtcManager::initializePeerConnection()
{
  rtc::Configuration config;
  config.iceServers.emplace_back("stun:stun.l.google.com:19302");

  pc = std::make_shared<rtc::PeerConnection>(config);
  // Setup ICE Candidate ghatering
  pc->onLocalCandidate(
      [this](rtc::Candidate candidate)
      {
        std::println("🧊 [WebRtcManager] Found ICE Candidate, sending to signaling server... ");
        json msg;
        msg["type"] = SignalingMessageType::Candidate;
        msg["target"] = "client"; // Send to the web browser
        msg["candidate"] = candidate.candidate();
        msg["sdpMid"] = candidate.mid();

        ws->send(msg.dump());
      });
}

void WebRtcManager::initializeDataChannel()
{
  if (!pc)
    throw std::runtime_error("Peer connection not initialized");
  dataChannel = pc->createDataChannel("input");
  dataChannel->onOpen([]()
                      { std::println("✅ [WebRtcManager] Input DataChannel Opened!"); });
  dataChannel->onMessage(
      [](std::variant<rtc::binary, rtc::string> data)
      {
        if (std::holds_alternative<rtc::binary>(data)) {
            // No futuro, aqui receberemos os pacotes binários do navegador
            // e vamos repassá-los para o /dev/uinput do Linux!
            std::println("🖱️ [WebRtcManager] Received binary input packet!"); 
        } else if (std::holds_alternative<rtc::string>(data)) {
            std::string text = std::get<rtc::string>(data);
            std::println("📝 [WebRtcManager] Received text on DataChannel: {}", text);   
        } });
}

void WebRtcManager::initializeVideoTrack(){
  if (!dataChannel)
    throw std::runtime_error("Data channel not initialized");
  rtc::Description::Video videoMedia("video", rtc::Description::Direction::SendOnly);
  videoMedia.addH264Codec(96); // 96 is universal standart for H.264

  auto videoTrack = pc->addTrack(videoMedia);
}

void WebRtcManager::sendVideoPacket(const uint8_t *data, size_t size)
{
  if (videoTrack && videoTrack->isOpen())
  {
    // A libdatachannel exige os dados em formato rtc::binary (std::byte)
    rtc::binary packet_data(reinterpret_cast<const std::byte *>(data),
                            reinterpret_cast<const std::byte *>(data) + size);
    videoTrack->send(packet_data);
  }
}

void WebRtcManager::connectSignaling(const std::string &url)
{
  ws = std::make_shared<rtc::WebSocket>();

  ws->onOpen([this]()
             {std::println("🌐 [WebRtcManager] Connected to Signaling Server!");
              // Send our registration JSON (can use raw string literal for simplicity)
              json regMsg;
              regMsg["type"] = SignalingMessageType::Register;
              regMsg["id"] = "Core-Engine";
              ws->send(regMsg.dump()); });

  // Listen for incoming SDP / ICE messages from the Client
  ws->onMessage(
      [this](std::variant<rtc::binary, rtc::string> data)
      {
        if (!std::holds_alternative<rtc::string>(data))
          return;

        std::string msg = std::get<rtc::string>(data);
        std::println("📩 [WebRtcManager] Received message: {}", msg);

        auto parsedJson = json::parse(msg);
        SignalingMessageType type = parsedJson["type"].get<SignalingMessageType>();
        switch (type)
        {
        case SignalingMessageType::Offer:
        {
          std::println("🤝 [WebRtcManager] Received SDP Offer from Client!");
          std::string sdpString = parsedJson["sdp"].get<std::string>();
          rtc::Description desc(sdpString, "offer");
          pc->setRemoteDescription(desc);

          auto answer = pc->localDescription();
          json reply;
          reply["type"] = SignalingMessageType::Answer;
          reply["target"] = "client";
          reply["sdp"] = std::string(*answer);
          ws->send(reply.dump());
          break;
        }
        case SignalingMessageType::Candidate:
        {
          std::println("🧊 [WebRtcManager] Received ICE Candidate from Client");
          std::string candString = parsedJson["candidate"].get<std::string>();
          std::string midString = parsedJson["sdpMid"].get<std::string>();
          rtc::Candidate candidate(candString, midString);
          pc->addRemoteCandidate(candidate);
          break;
        }

        default:
          std::println("⚠️ [WebRtcManager] Unknown message type");
          break;
        }
      });

  ws->onClosed([]()
               { std::println("❌ [WebRtcManager] Disconnected from Signaling Server."); });

  ws->onError([](const std::string &err)
              { std::println("❌ [WebRtcManager] WebSocket Error: {}", err); });

  // Actually open the connection
  std::println("🌐 [WebRtcManager] Connecting to {}...", url);
  ws->open(url);
}
