#include "WebRtcManager.hpp"

WebRtcManager::WebRtcManager() { rtc::InitLogger(rtc::LogLevel::Info); }

WebRtcManager::~WebRtcManager()
{
  if (ws)
    ws->close();
}

void WebRtcManager::connectSignaling(const std::string &url)
{ // When connection opens, register this engine as the "host"
  ws = std::make_shared<rtc::WebSocket>();

  ws->onOpen([this]()
             {std::println("🌐 [WebRtcManager] Connected to Signaling Server!");
              // Send our registration JSON (can use raw string literal for simplicity)  
              std::string regMsg = R"({"type": "register", "id": "host"})";              
              ws->send(regMsg); });

  // Listen for incoming SDP / ICE messages from the Client
  ws->onMessage([](std::variant<rtc::binary, rtc::string> data)
                {                
                  if (std::holds_alternative<rtc::string>(data)) {                           
                      std::string msg = std::get<rtc::string>(data);                         
                      std::println("📩 [WebRtcManager] Received message: {}", msg);
                      // In Step 4, we will parse this JSON to see if it's an "offer" or "candidate"
                  ;} });

  ws->onClosed([]()
               { std::println("❌ [WebRtcManager] Disconnected from Signaling Server."); });

  ws->onError([](const std::string &err)
              { std::println("❌ [WebRtcManager] WebSocket Error: {}", err); });

  // Actually open the connection
  std::println("🌐 [WebRtcManager] Connecting to {}...", url);
  ws->open(url);
}
