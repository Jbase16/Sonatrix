#pragma once

#include "VoiceManager.h"

#include <string>

namespace Sonatrix {
namespace Core {
namespace Audio {

class BassVoiceManager : public VoiceManager {
public:
  BassVoiceManager() = default;
  ~BassVoiceManager() override = default;

  bool LoadElectricBassKit(const std::string &assetsAbsolutePath);
  bool LoadMockBassKit(const std::string &assetsAbsolutePath);
  void ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) override;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
