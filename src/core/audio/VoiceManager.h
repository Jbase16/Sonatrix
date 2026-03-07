#pragma once

#include "../midi/MIDIEvent.h"
#include "AudioVoice.h"
#include "AudioMixer.h"
#include <array>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Audio {

// -----------------------------------------------------------------------------
// VoiceManager (Polyphony & Note Stealing)
//
// Allocates a fixed pool of lock-free voices.
// Routes incoming MIDI NoteOn/Off events to free voices.
// Exectues priority-based note stealing if max polyphony is reached.
// -----------------------------------------------------------------------------

class VoiceManager {
public:
  VoiceManager() = default;
  ~VoiceManager() = default;

  // Process an incoming MIDI buffer (prepared by Phase 3 Engines)
  // Note: This must be called from the real-time audio thread
  void ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events,
                   InstrumentArticulation &articulation);

  // Renders all active voices into the given output buffer
  void RenderAudio(float **outputChannels, uint32_t numFrames,
                   uint32_t numChannels);

  // Phase 10: Multi-Sampler Kit Loader
  // Loads physical .wav files from the given absolute directory path
  void LoadInstrumentKit(const std::string &assetsAbsolutePath);
  InstrumentArticulation &GetKitArticulation() { return activeArticulation_; }

  AudioMixer& GetMixer() { return mixer_; }

private:
  // Max polyphony constraint (e.g., 64 stereo voices)
  static constexpr size_t MAX_VOICES = 64;
  std::array<AudioVoice, MAX_VOICES> voices_;

  InstrumentArticulation activeArticulation_;

  // Finds the best voice to use (either truly Free, or by stealing the lowest
  // priority active voice)
  AudioVoice *GetBestAvailableVoice();
  
  AudioMixer mixer_;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
