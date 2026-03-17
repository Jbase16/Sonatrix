#pragma once

#include "../midi/MIDIEvent.h"
#include "AudioMixer.h"
#include "SamplerVoice.h"
#include "SampleZone.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Audio {

// Shared polyphonic voice container and renderer.
// Instrument-specific managers own loading and MIDI semantics on top of this.
class VoiceManager {
public:
  VoiceManager() = default;
  virtual ~VoiceManager() = default;

  virtual void ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) = 0;

  void RenderAudio(float **outputChannels, uint32_t numFrames,
                   uint32_t numChannels);

  AudioMixer &GetMixer() { return mixer_; }
  const AudioMixer &GetMixer() const { return mixer_; }
  const InstrumentArticulation &GetKitArticulation() const {
    return activeArticulation_;
  }
  MixerBus GetMixerBus() const { return activeMixerBus_; }

protected:
  static constexpr size_t MAX_VOICES = 32;

  bool LoadArticulation(const InstrumentArticulation &articulation,
                        MixerBus mixerBus);
  bool HasLoadedArticulation() const { return !activeArticulation_.zones.empty(); }

  bool StartVoiceForEvent(const MIDI::MIDIEvent &event, int stringId = -1);
  void StopVoiceForEvent(const MIDI::MIDIEvent &event, int stringId = -1);
  void ChokeVoicesOnString(int stringId);
  void ResetRuntimeState();

private:
  SamplerVoice *GetBestAvailableVoice();

  std::array<SamplerVoice, MAX_VOICES> voices_;
  InstrumentArticulation activeArticulation_;
  float articulationOutputGain_{1.0f};
  AudioMixer mixer_;
  MixerBus activeMixerBus_{MixerBus::Bass};
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
