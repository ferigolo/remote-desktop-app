#pragma once
#include <rtc/rtc.hpp>
#include <memory>
#include <string>
#include <print>

class WebRtcManager
{
private:
  // libdatachannel's built-in WebSocket client
  std::shared_ptr<rtc::WebSocket> ws;

public:
  WebRtcManager();
  ~WebRtcManager();

  // Connects to the Python Signaling Server
  void connectSignaling(const std::string &url);
};