// C-ABI Bridge

#include <print>
#include <exception>

class MediaEngine
{
public:
  [[nodiscard]] bool initialize(); // nodiscard -> compiler flag to set a warning if return value was not used
};

// Disable C++ name mangling to provide C linkage.
// This allows the function to be called from C code or other languages using FFI
extern "C"
{
  static MediaEngine *g_engine = nullptr;
  bool start_media_engine();
}