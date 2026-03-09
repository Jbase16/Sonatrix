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

  void Start(const SampleZone *zone, uint8_t targetPitch, double pitchRatio,
             float velocity);

  void Stop();

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
    double internalReadPos = 0.0;
    double durationSamples = 0.0;
    double ageSamples = 0.0;
    bool active = false;
  };

  static constexpr size_t MAX_GRAINS = 8;

  // Core note state
  std::atomic<State> state_{State::Free};
  const SampleZone *activeZone_{nullptr};
  uint8_t currentPitch_{0};
  float currentVelocity_{0.0f};

  // Time model
  double masterTimePos_{0.0};
  double directReadPos_{0.0};
  double granularMacroPos_{0.0};
  double timeAdvanceRate_{1.0};
  double pitchRatio_{1.0};

  // PSOLA / Granular Model
  double grainReadSpeed_{1.0};

  // Fast, lock-free PRNG state for the DSP thread
  uint32_t prngState_{881726454};

  // Envelope / release
  float envelopeLevel_{0.0f};
  double releasePos_{0.0};
  double releaseSamples_{44100.0 * 0.05};
  float releaseStartAmp_{1.0f};

  // Hybrid attack -> sustain crossfade
  double attackBypassSamples_{0.0};
  double transitionSamples_{0.0};
  bool sustainSeeded_{false};

  // Granular scheduler
  std::array<Grain, MAX_GRAINS> grains_{};
  double samplesUntilNextGrain_{0.0};
  double hopSizeSamples_{0.0};
  double nominalGrainDurationSamples_{0.0};

  void ResetGrains();
  bool HasActiveGrains() const;
  void SpawnGrain(size_t maxSourceFrames);
  StereoSample ReadInterpolated(double readPos) const;
  StereoSample RenderGranularSample(size_t maxSourceFrames);
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix