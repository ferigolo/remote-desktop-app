// C-ABI Bridge

#include <print>
#include <exception>

class MediaEngine
{
public:
  bool initialize();
};

typedef void (*EventCallback)();

// Disable C++ name mangling to provide C linkage.
// This allows the function to be called from C code or other languages using FFI
extern "C"
{
  static MediaEngine *gEngine = nullptr;
  bool start_media_engine(EventCallback onCloseCallback);
}