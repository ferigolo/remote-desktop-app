#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <mutex>
#include <vector>

class ClientWebRtcManager;
class H264Decoder;

class ClientApp {
public:
    ClientApp();
    ~ClientApp();

    bool initialize(const std::string& signalingUrl);
    void run();

private:
    void renderFrame(const uint8_t* rgbaData, int width, int height, int pitch);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    int texWidth = 0;
    int texHeight = 0;

    std::unique_ptr<ClientWebRtcManager> rtcManager;
    std::unique_ptr<H264Decoder> decoder;
    bool isRunning = false;

    // Cross-thread rendering synchronization
    std::mutex frameMutex;
    std::vector<uint8_t> frameBuffer;
    int framePitch = 0;
    bool frameReady = false;
};
