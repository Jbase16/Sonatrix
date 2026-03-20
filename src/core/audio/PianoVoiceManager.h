#pragma once

#include "VoiceManager.h"
#include <string>

namespace Sonatrix {
namespace Core {
namespace Audio {

class PianoVoiceManager : public VoiceManager {
public:
  PianoVoiceManager() = default;
  ~PianoVoiceManager() override = default;

  bool LoadAcousticPianoKit(const std::string &assetsAbsolutePath);
  
  // Custom processing to eventually handle standard sustain pedal CC #64
  void ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) override;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
