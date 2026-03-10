#pragma once

#include "../midi/MIDIEvent.h"
#include "AudioMixer.h"
#include "SamplerVoice.h"
#include "SampleZone.h"

#include <array>
#include <string>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Audio {

// -----------------------------------------------------------------------------
// VoiceManager
//
// Represents the synthesis brain for a single conceptual instrument
// (e.g., "The Bass Engine", "The Guitar Engine").
// It handles mapping incoming MIDI events to physical SampleZones,
// allocating polyphonic voices, and summing their outputs.
// -----------------------------------------------------------------------------

class VoiceManager {
public:
  VoiceManager() = default;
  ~VoiceManager() = default;

  // Loads a sparse matrix of samples into RAM for this specific engine.
  void LoadInstrumentKit(const std::string &assetsAbsolutePath);

  // Receives a stream of timestamped MIDI events.
  // In real-time use, this should only receive events meant for the *current*
  // audio block.
  void ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events,
                   InstrumentArticulation &articulation);

  // Pulls the next N samples from all active voices, sums them together,
  // passes them through the mixer for gain/pan staging, and accumulates
  // the result into outputChannels.
  void RenderAudio(float **outputChannels, uint32_t numFrames,
                   uint32_t numChannels);

private:
  // Polyphony cap. Expanded to 32 to handle overlapping 6-string acoustic strums without aggressive choking.
  static constexpr size_t MAX_VOICES = 32;

  std::array<SamplerVoice, MAX_VOICES> voices_;
  InstrumentArticulation activeArticulation_;
  AudioMixer mixer_;

  // Finds a free voice slot. If all slots are full, returns the active
  // voice with the lowest amplitude (stealing).
  SamplerVoice *GetBestAvailableVoice();
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix