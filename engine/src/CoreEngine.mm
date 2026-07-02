#include "CoreEngine.hpp"
#include "configurers/MediaEngineConfigurer.hpp"
#include <SDL3/SDL.h>
#include <print>
#include <thread>
#include <cmath>
#include <string_view>
#include <SDL3/SDL_opengl.h>

#ifdef __APPLE__
#include <SDL3/SDL_metal.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#endif

MediaEngine::MediaEngine() : window(nullptr), renderer(nullptr), is_running(false) {}

MediaEngine::~MediaEngine()
{
  cleanup();
}

bool MediaEngine::initialize()
{
  MediaEngineConfigurer::configSDL();

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    std::println(stderr, "❌ [CoreEngine] Error initializing SDL: {}", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("Remote Desktop", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MAXIMIZED);
  if (!window)
  {
    std::println(stderr, "❌ [CoreEngine] Error creating window: {}", SDL_GetError());
    cleanup();
    return false;
  }

  // SDL3 no longer uses flags for hardware acceleration/software in CreateRenderer.
  // It handles it natively or you pass a specific driver string. Passing NULL gets the best default.
  renderer = SDL_CreateRenderer(window, NULL);
  SDL_SetRenderVSync(renderer, 1);

  if (!renderer)
  {
    std::println(stderr, "❌ [CoreEngine] Error creating renderer: {}", SDL_GetError());
    cleanup();
    return false;
  }
  std::println(" [Core] Renderer created successfully");
  printRendererInfo();
  is_running = true;

  capturer = ScreenCapturer::create();
  capturer->start([](const VideoFrame &frame)
                  { static unsigned int count = 0;
                     std::println("Got frame {}!", ++count); });
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
      if (event.type == SDL_EVENT_QUIT)
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

void MediaEngine::printRendererInfo() const
{
#ifndef NDEBUG
  if (!renderer)
    return;
  const char *backendName = SDL_GetRendererName(renderer);
  std::println(" [Core] Using {} backend", backendName);

  std::string_view backend(backendName);

  #ifdef __linux__
  if (backend == "opengl")
  {
    auto glGetStringFunc = (const GLubyte *(*)(GLenum))
        SDL_GL_GetProcAddress("glGetString");

    if (glGetStringFunc)
    {
      const char *gpuName = reinterpret_cast<const char *>(glGetStringFunc(GL_RENDERER));
      const char *glVersion = reinterpret_cast<const char *>(glGetStringFunc(GL_VERSION));

      std::println("\n🎮 Using GPU: {}", gpuName ? gpuName : "Unknown");
      std::println("⚙️  OpenGL version: {}\n", glVersion ? glVersion : "Unknown");
    }
  }
  #elif defined(__APPLE__)
  if (backend == "metal")
  {
    SDL_MetalView view = SDL_Metal_CreateView(window);
    CAMetalLayer *layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(view);

    if (layer && layer.device)
    {
      id<MTLDevice> device = layer.device;
      const char *gpuName = [device.name UTF8String];
      std::println("🍎 Maped GPU (Metal): {}", gpuName);
    }
    SDL_Metal_DestroyView(view);
  }
  #endif
#endif
}