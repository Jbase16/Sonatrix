#pragma once

#include "SampleZone.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace Sonatrix {
namespace Core {
namespace Audio {

class GranularVoice {
public:
  GranularVoice() = default;
  ~GranularVoice() = default;

  // Called when a MIDI NoteOn is received.
  // pitchRatio dictates the pitch shift (e.g. 2.0 = octave up).
  // targetPitch is used for voice management / stealing.
  void Start(const SampleZone *zone, uint8_t targetPitch, double pitchRatio,
             float velocity);

  // Triggers the release phase of the envelope.
  void Stop();

  // Renders hybrid direct-attack + granular-sustain output.
  void RenderNextBlock(float **outputChannels, uint32_t numFrames,
                       uint32_t numChannels);

  bool IsFree() const { return state_ == State::Free; }
  uint8_t GetCurrentPitch() const { return currentPitch_; }
  float GetStealingPriority() const { return envelopeLevel_; }

private:
  enum class State { Free, Active, Releasing };

  struct StereoSample {
    float left = 0.0f;
    float right = 0.0f;
  };

  struct Grain {
    double internalReadPos = 0.0; // current playback head for this grain
    double durationSamples = 0.0; // grain lifespan
    double ageSamples = 0.0;      // how old the grain is
    bool active = false;
  };

  static constexpr size_t MAX_GRAINS = 8;

  // Core note state
  std::atomic<State> state_{State::Free};
  const SampleZone *activeZone_{nullptr};
  uint8_t currentPitch_{0};
  float currentVelocity_{0.0f};

  // Time model
  double masterTimePos_{0.0};    // sustain timeline, advances strictly at 1.0x
  double directReadPos_{0.0};    // attack playback head, advances at pitchRatio
  double granularMacroPos_{0.0}; // macro playback head for grains
  double timeAdvanceRate_{1.0};  // how fast the granular macro head advances
  double pitchRatio_{1.0};       // overall pitch shift ratio

  // PSOLA Model
  double rootPeriodSamples_{1.0};
  double targetPeriodSamples_{1.0};
  double grainReadSpeed_{1.0};
  // Envelope / release
  float envelopeLevel_{0.0f};
  double releasePos_{0.0};
  double releaseSamples_{44100.0 * 0.05}; // 50 ms
  float releaseStartAmp_{1.0f};

  // Hybrid attack -> sustain crossfade
  double attackBypassSamples_{0.0}; // direct-only region
  double transitionSamples_{0.0};   // crossfade region
  bool sustainSeeded_{false};

  // Granular scheduler
  std::array<Grain, MAX_GRAINS> grains_{};
  double samplesUntilNextGrain_{0.0};
  double hopSizeSamples_{0.0};
  double nominalGrainDurationSamples_{0.0};
  uint32_t grainCounter_{0};

  // Helpers
  void ResetGrains();
  bool HasActiveGrains() const;
  void SpawnGrain(size_t maxSourceFrames);
  StereoSample ReadInterpolated(double readPos) const;
  StereoSample RenderGranularSample(size_t maxSourceFrames, float &outNorm);
  double NextRand01();
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix