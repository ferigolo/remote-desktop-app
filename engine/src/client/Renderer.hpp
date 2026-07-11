#pragma once
#include <mutex>

extern "C" {
#include <SDL3/SDL_render.h>
#include <libavutil/frame.h>
}

#ifdef HAVE_CUDA
#include <GL/gl.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#endif

class Renderer {
 public:
  Renderer();
  ~Renderer();

  bool initialize();
  void renderFrame();
  void setFrame(AVFrame* newFrame);

 private:
  AVFrame* frame = nullptr;
  SDL_Renderer* sdlRenderer{};
  SDL_Window* window{};
  SDL_Texture* texture{};

  int texWidth = 0;
  int texHeight = 0;

#ifdef HAVE_CUDA
  cudaGraphicsResource_t cudaResY = nullptr;
  cudaGraphicsResource_t cudaResUV = nullptr;
#endif

  // Cross-thread rendering synchronization
  std::mutex frameMutex;
  bool frameReady = false;
  bool isInitialized = false;

  void updateOnDimensionsChange();
  void handleFrameFormat();
  void handleCudaFormat();
};
