#pragma once
#include <memory>

class CoreEngineConfigurer
{
public:
  virtual ~CoreEngineConfigurer() = default;
  static void configSDL();
};
