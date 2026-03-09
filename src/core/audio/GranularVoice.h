#pragma once

#include "SampleZone.h"
#include <array>
#include <atomic>
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace Audio {

class GranularVoice {
public:
  GranularVoice() = default;
  ~GranularVoice() = default;

  // Called when a MIDI NoteOn is received.
  // pitchRatio dictates the pitch shift (e.g. 2.0 = octave up).
  // Target pitch is strictly for voice stealing / reference.
  void Start(const SampleZone *zone, uint8_t targetPitch, double pitchRatio,
             float velocity);

  // Triggers the Release phase of the ADSR envelope.
  void Stop();

  // Renders the overlapping granular grains
  void RenderNextBlock(float **outputChannels, uint32_t numFrames,
                       uint32_t numChannels);

  // Status checks
  bool IsFree() const { return state_ == State::Free; }
  uint8_t GetCurrentPitch() const { return currentPitch_; }
  float GetStealingPriority() const { return envelopeLevel_; }

private:
  enum class State { Free, Active, Releasing };
  std::atomic<State> state_{State::Free};

  const SampleZone *activeZone_{nullptr};
  uint8_t currentPitch_{0};
  float currentVelocity_{0.0f};

  // Master time advancement through the sample (advances at 1.0x)
  double masterTimePos_{0.0};

  // The pitch ratio applied to the inside of the grains
  double pitchRatio_{1.0};

  // ADSR Envelope
  float envelopeLevel_{0.0f};
  float envelopeTarget_{0.0f};
  double releasePos_{0.0};
  double releaseSamples_{44100.0 * 0.05}; // 50ms quick release
  float releaseStartAmp_{1.0f};

  struct Grain {
    double internalReadPos = 0.0;
    double durationSamples = 0.0;
    double ageSamples = 0.0;
    bool active = false;
  };

  // 4 overlapping grains is typical for granular
  static constexpr size_t MAX_GRAINS = 4;
  std::array<Grain, MAX_GRAINS> grains_{};

  double samplesUntilNextGrain_{0.0};
  double hopSizeSamples_{0.0};
  double grainDurationSamples_{0.0};
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
