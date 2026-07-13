#include "Renderer.hpp"

#include <cstdio>
#include <print>

Renderer::Renderer() {}

Renderer::~Renderer() {
#ifdef HAVE_CUDA
  if (cudaResY) cudaGraphicsUnregisterResource(cudaResY);
  if (cudaResUV) cudaGraphicsUnregisterResource(cudaResUV);
#endif
  if (frame) av_frame_free(&frame);
  if (texture) SDL_DestroyTexture(texture);
  if (sdlRenderer) SDL_DestroyRenderer(sdlRenderer);
  if (window) SDL_DestroyWindow(window);
}

bool Renderer::initialize() {
  window =
      SDL_CreateWindow("Remote Desktop Client - Native", 1920, 1080,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                           SDL_WINDOW_INPUT_FOCUS);
  if (!window) return false;

  // We must use OpenGL for CUDA-GL interop
  sdlRenderer = SDL_CreateRenderer(window, "opengl");
  if (!sdlRenderer) {
    std::println(stderr,
                 "Warning: OpenGL renderer failed, falling back to default.");
    sdlRenderer = SDL_CreateRenderer(window, nullptr);
  }

  int hostWidth = 1920, hostHeight = 1080;
  if (!SDL_SetRenderLogicalPresentation(sdlRenderer, hostWidth, hostHeight,
                                        SDL_LOGICAL_PRESENTATION_LETTERBOX))
    std::println(stderr, "Warning: Letterboxing failed: {}", SDL_GetError());

  isInitialized = true;
  return true;
}

void Renderer::setFrame(AVFrame* newFrame) {
  std::lock_guard<std::mutex> lock(frameMutex);
  if (frame) av_frame_free(&frame);
  // av_frame_clone manages reference counting properly for both HW and CPU
  // frames
  frame = av_frame_clone(newFrame);
  frameReady = true;
}

void Renderer::renderFrame() {
  std::lock_guard<std::mutex> lock(frameMutex);

  if (frame && frameReady) {
    updateOnDimensionsChange();
    handleFrameFormat();
    frameReady = false;
  }

  // Draw to screen
  SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
  SDL_RenderClear(sdlRenderer);
  if (texture) SDL_RenderTexture(sdlRenderer, texture, nullptr, nullptr);
  SDL_RenderPresent(sdlRenderer);
}

void Renderer::handleFrameFormat() {
  if (frame->format == AV_PIX_FMT_CUDA) {
    handleCudaFormat();
  } else  // Standard Software Mapping (NV12) (Used for Apple Silicon too)
    SDL_UpdateNVTexture(texture, nullptr, frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1]);
}

// Recreate texture if dimensions changed
void Renderer::updateOnDimensionsChange() {
  if (!texture || texWidth != frame->width || texHeight != frame->height) {
    if (texture) {
#ifdef HAVE_CUDA
      if (cudaResY) cudaGraphicsUnregisterResource(cudaResY);
      if (cudaResUV) cudaGraphicsUnregisterResource(cudaResUV);
      cudaResY = nullptr;
      cudaResUV = nullptr;
#endif
      SDL_DestroyTexture(texture);
    }
    texWidth = frame->width;
    texHeight = frame->height;
    texture =
        SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_NV12,
                          SDL_TEXTUREACCESS_STREAMING, texWidth, texHeight);

#ifdef HAVE_CUDA
    // Extract raw OpenGL texture handles from SDL3
    SDL_PropertiesID props = SDL_GetTextureProperties(texture);
    GLuint texY = (GLuint)SDL_GetNumberProperty(
        props, SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER, 0);
    GLuint texUV = (GLuint)SDL_GetNumberProperty(
        props, SDL_PROP_TEXTURE_OPENGL_TEXTURE_UV_NUMBER, 0);

    // Register the OpenGL textures with CUDA for writing
    cudaGraphicsGLRegisterImage(&cudaResY, texY, GL_TEXTURE_2D,
                                cudaGraphicsRegisterFlagsWriteDiscard);
    cudaGraphicsGLRegisterImage(&cudaResUV, texUV, GL_TEXTURE_2D,
                                cudaGraphicsRegisterFlagsWriteDiscard);
#endif
  }
}

void Renderer::handleCudaFormat() {
#ifdef HAVE_CUDA
  if (!cudaResY || !cudaResUV) return;

  // Map the OpenGL textures to CUDA arrays
  cudaGraphicsMapResources(1, &cudaResY);
  cudaGraphicsMapResources(1, &cudaResUV);

  cudaArray_t arrayY, arrayUV;
  cudaGraphicsSubResourceGetMappedArray(&arrayY, cudaResY, 0, 0);
  cudaGraphicsSubResourceGetMappedArray(&arrayUV, cudaResUV, 0, 0);

  // Perform a Device-to-Device copy (VRAM -> VRAM)
  // Copy Y Plane
  cudaMemcpy2DToArrayAsync(arrayY, 0, 0, frame->data[0], frame->linesize[0],
                           texWidth, texHeight, cudaMemcpyDeviceToDevice);
  // Copy UV Plane (NV12 UV plane is half height)
  cudaMemcpy2DToArrayAsync(arrayUV, 0, 0, frame->data[1], frame->linesize[1],
                           texWidth, texHeight / 2, cudaMemcpyDeviceToDevice);

  // Unmap and synchronize
  cudaGraphicsUnmapResources(1, &cudaResUV);
  cudaGraphicsUnmapResources(1, &cudaResY);
  cudaStreamSynchronize(0);  // Ensure the copy finishes before SDL renders it
#endif
}