#include "VoiceManagerFactory.h"

#include "BassVoiceManager.h"
#include "GuitarVoiceManager.h"
#include "PianoVoiceManager.h"
#include "VoiceManager.h"

#include <iostream>

namespace Sonatrix {
namespace Core {
namespace Audio {

std::unique_ptr<VoiceManager>
CreateLoadedVoiceManager(PlaybackInstrument instrument,
                         const std::string &assetsAbsolutePath) {
  if (assetsAbsolutePath.empty()) {
    std::cerr << "VoiceManagerFactory: empty asset path for instrument "
              << static_cast<int>(instrument) << std::endl;
    return nullptr;
  }

  switch (instrument) {
  case PlaybackInstrument::Guitar: {
    auto manager = std::make_unique<GuitarVoiceManager>();
    if (!manager->LoadAcousticGuitarKit(assetsAbsolutePath)) {
      std::cerr << "VoiceManagerFactory: failed to load guitar kit from "
                << assetsAbsolutePath << std::endl;
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::ElectricBass: {
    auto manager = std::make_unique<BassVoiceManager>();
    if (!manager->LoadElectricBassKit(assetsAbsolutePath)) {
      std::cerr << "VoiceManagerFactory: failed to load electric bass kit from "
                << assetsAbsolutePath << std::endl;
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::MockBass: {
    auto manager = std::make_unique<BassVoiceManager>();
    if (!manager->LoadMockBassKit(assetsAbsolutePath)) {
      std::cerr << "VoiceManagerFactory: failed to load mock bass kit from "
                << assetsAbsolutePath << std::endl;
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::AcousticPiano: {
    auto manager = std::make_unique<PianoVoiceManager>();
    if (!manager->LoadAcousticPianoKit(assetsAbsolutePath)) {
      std::cerr << "VoiceManagerFactory: failed to load piano kit from "
                << assetsAbsolutePath << std::endl;
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
