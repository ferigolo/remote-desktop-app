#pragma once
#include <memory>

class MediaEngineConfigurer
{
public:
  virtual ~MediaEngineConfigurer() = default;
  static void configSDL();
};
