#include "DecoderFactory.hpp"

#include <memory>

#include "software/SoftwareDecoder.hpp"

#ifdef HAVE_NVIDIA
#include "nvidia/NvidiaDecoder.hpp"
#endif

#ifdef HAVE_INTEL_QSV
#include "intel/IntelIrisDecoder.hpp"
#endif

#ifdef HAVE_APPLE_VT
#include "apple/AppleDecoder.hpp"
#endif

std::unique_ptr<BaseDecoder> createOptimalDecoder() {
#ifdef HAVE_APPLE_VT
  // On Mac, VideoToolbox is almost always the right choice
  return std::make_unique<AppleDecoder>();
#endif
#ifdef HAVE_NVIDIA
  if (avcodec_find_decoder_by_name("h264_cuvid")) {
    return std::make_unique<NvidiaDecoder>();
  }
#endif
#ifdef HAVE_INTEL_QSV
  if (avcodec_find_decoder_by_name("h264_qsv")) {
    return std::make_unique<IntelIrisDecoder>();
  }
#endif
  // Fallback to Software
  return std::make_unique<SoftwareDecoder>();
}