#pragma once
#include <SDL2/SDL.h>
#include <atomic>

class MediaEngine
{
public:
  MediaEngine();
  ~MediaEngine();

  // Inicia a janela e o render loop
  bool initialize();

private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  std::atomic<bool> is_running;

  void render_loop();
  void cleanup();
};