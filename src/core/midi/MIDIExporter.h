#pragma once

#include "MIDIEvent.h"
#include <string>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace MIDI {

class MIDIExporter {
public:
  // Exports standard Type 0 MIDI file format mapping directly from the MIR
  // compilers
  static bool ExportToSMF(const std::string &outputPath,
                          const std::vector<MIDIEvent> &midiStream,
                          uint16_t ppq = static_cast<uint16_t>(STANDARD_PPQN));
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
