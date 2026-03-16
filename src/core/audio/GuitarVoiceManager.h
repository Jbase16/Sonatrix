#pragma once

#include "VoiceManager.h"

#include <array>
#include <string>

namespace Sonatrix {
namespace Core {
namespace Audio {

class GuitarVoiceManager : public VoiceManager {
public:
  GuitarVoiceManager() = default;
  ~GuitarVoiceManager() override = default;

  bool LoadAcousticGuitarKit(const std::string &assetsAbsolutePath);
  void ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) override;

private:
  void ResetStringState();

  std::array<int, 6> stringActiveNotes_{0, 0, 0, 0, 0, 0};
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
