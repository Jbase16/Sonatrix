#pragma once

#include "../midi/MIDIEvent.h"
#include "VoiceManager.h"

#include <string>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Audio {

class AudioExporter {
public:
  static bool BounceOffline(
      const std::string &outputPath,
      const std::vector<Sonatrix::Core::MIDI::MIDIEvent> &midiStream,
      const std::string &assetsPath, PlaybackInstrument instrument,
      const std::vector<float> &busVolumes, double sampleRate = 44100.0,
      double tempoBPM = 120.0);
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
