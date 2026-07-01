#include "CoreEngine.hpp"
#include <print>
#include <thread>
#include <cmath>

MediaEngine::MediaEngine() : window(nullptr), renderer(nullptr), is_running(false) {}

MediaEngine::~MediaEngine()
{
  cleanup();
}

bool MediaEngine::initialize()
{
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    std::println(stderr, "❌ [CoreEngine] Error initializing SDL: {}", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("Remote Desktop", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);
  if (!window)
  {
    std::println(stderr, "❌ [CoreEngine] Erro ao criar janela: {}", SDL_GetError());
    cleanup();
    return false;
  }

  // SDL_RENDERER_ACCELERATED -> ensures GPU use
  // SDL_RENDERER_PRESENTVSYNC -> locks the renderer into monitor framerate
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
  {
    std::println(stderr, "❌ [CoreEngine] Error creating GPU renderer: {}", SDL_GetError());
    cleanup();
    return false;
  }

  std::println("🚀 [C++26 Core] GPU accelerated window created successfully");
  is_running = true;
  render_loop();

  return true;
}

void MediaEngine::render_loop()
{
  SDL_Event event;
  Uint32 start_time = SDL_GetTicks();

  while (is_running)
  {
    while (SDL_PollEvent(&event)) // poll until all events are handled
    {
      if (event.type == SDL_QUIT)
      {
        is_running = false;
        std::println("🛑 [CoreEngine] Quit window signal received");
      }
      // TODO: capture mouse and keyboard events
    }

    Uint32 current_time = SDL_GetTicks() - start_time;
    Uint8 red = static_cast<Uint8>((std::sin(current_time * 0.001) + 1.0) * 127.5);
    Uint8 green = static_cast<Uint8>((std::sin(current_time * 0.002) + 1.0) * 127.5);
    Uint8 blue = static_cast<Uint8>((std::sin(current_time * 0.003) + 1.0) * 127.5);

    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    SDL_RenderClear(renderer);

    SDL_RenderPresent(renderer);
  }

  cleanup();
}

void MediaEngine::cleanup()
{
  if (renderer)
  {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window)
  {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  SDL_Quit();
  std::println("🧹 [CoreEngine] Released resources");
}
