#include "VoiceManagerFactory.h"

#include "BassVoiceManager.h"
#include "GuitarVoiceManager.h"
#include "VoiceManager.h"

namespace Sonatrix {
namespace Core {
namespace Audio {

std::unique_ptr<VoiceManager>
CreateLoadedVoiceManager(PlaybackInstrument instrument,
                         const std::string &assetsAbsolutePath) {
  switch (instrument) {
  case PlaybackInstrument::Guitar: {
    auto manager = std::make_unique<GuitarVoiceManager>();
    if (!assetsAbsolutePath.empty() &&
        !manager->LoadAcousticGuitarKit(assetsAbsolutePath)) {
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::ElectricBass: {
    auto manager = std::make_unique<BassVoiceManager>();
    if (!assetsAbsolutePath.empty() &&
        !manager->LoadElectricBassKit(assetsAbsolutePath)) {
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::MockBass: {
    auto manager = std::make_unique<BassVoiceManager>();
    if (!assetsAbsolutePath.empty() &&
        !manager->LoadMockBassKit(assetsAbsolutePath)) {
      return nullptr;
    }
    return manager;
  }
  }

  return nullptr;
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
