#pragma once
#include <memory>

#include "BaseDecoder.hpp"

std::unique_ptr<BaseDecoder> createOptimalDecoder();
