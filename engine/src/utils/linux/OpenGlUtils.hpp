#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <format>
#include <vector>

namespace OpenGlUtils {

inline EGLDisplay getEglDisplay() {
  int fd = -1;
  for (int i = 128; i < 136; ++i) {
    std::string path = std::format("/dev/dri/renderD{}", i);
    int temp_fd = open(path.c_str(), O_RDWR);
    if (temp_fd >= 0) {
      drmVersionPtr version = drmGetVersion(temp_fd);
      if (version && version->name && std::string_view(version->name) == "nvidia-drm") {
        std::println("Found NVIDIA DRM node at {}", path);
        drmFreeVersion(version);
        fd = temp_fd;
        break;
      }
      if (version) drmFreeVersion(version);
      close(temp_fd);
    }
  }

  if (fd < 0) {
    std::println("Failed to find NVIDIA DRM render node, falling back to /dev/dri/renderD128");
    fd = open("/dev/dri/renderD128", O_RDWR);
  }

  if (fd < 0) {
    std::println("Failed to open DRM render node");
    return EGL_NO_DISPLAY;
  }

  // Create a GBM device from the file descriptor
  struct gbm_device* gbm = gbm_create_device(fd);
  if (!gbm) {
    std::println("Failed to create GBM device");
    return EGL_NO_DISPLAY;
  }

  // Get the EGLDisplay using the GBM platform extension
  EGLDisplay eglDisplay =
      eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm, NULL);

  if (eglDisplay == EGL_NO_DISPLAY) {
    std::println("Failed to get EGL display");
    return EGL_NO_DISPLAY;
  }

  // Initialize the display
  EGLint major, minor;
  if (!eglInitialize(eglDisplay, &major, &minor)) {
    std::println("Failed to initialize EGL");
    return EGL_NO_DISPLAY;
  }

  std::println("Successfully initialized EGL {}.{} via GBM", major, minor);
  return eglDisplay;
}

inline std::vector<uint64_t> getSupportedModifiers(EGLDisplay eglDispl,
                                            EGLint format) {
  // Assuming you already have an initialized EGLDisplay (egl_display)
  // EGLint format = DRM_FORMAT_XRGB8888;  // The format you want to capture
  EGLint numModifiers = 0;
  PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT =
      (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress(
          "eglQueryDmaBufModifiersEXT");

  if (!eglQueryDmaBufModifiersEXT) {
    std::println("eglQueryDmaBufModifiersEXT is not supported\n");
    return {};
  }

  // Ask EGL how many modifiers exist for this format
  eglQueryDmaBufModifiersEXT(eglDispl, format, 0, NULL, NULL, &numModifiers);

  if (numModifiers <= 0) return {};

  // Extract the actual modifiers
  std::vector<uint64_t> modifiers(numModifiers);
  eglQueryDmaBufModifiersEXT(eglDispl, format, numModifiers, modifiers.data(),
                             NULL, &numModifiers);

#ifndef NDEBUG
  std::println("Supported Modifiers for {}:", format);
  for (int i = 0; i < numModifiers; i++) std::println(" - {}", modifiers[i]);
#endif
  return modifiers;
}
}  // namespace OpenGlUtils
