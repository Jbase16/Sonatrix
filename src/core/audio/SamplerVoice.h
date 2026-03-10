#pragma once

#include "SampleZone.h"
#include <atomic>
#include <cstdint>

namespace Sonatrix {
namespace Core {
namespace Audio {

class SamplerVoice {
public:
  SamplerVoice() = default;
  ~SamplerVoice() = default;

  void Start(const SampleZone *zone, uint8_t targetPitch, double pitchRatio,
             float velocity, int stringId = -1);

  void Stop();

  void RenderNextBlock(float **outputChannels, uint32_t numFrames,
                       uint32_t numChannels);

  bool IsFree() const { return state_ == State::Free; }
  uint8_t GetCurrentPitch() const { return currentPitch_; }
  int GetStringId() const { return currentStringId_; }
  float GetStealingPriority() const { return envelopeLevel_; }

private:
  enum class State { Free, Active, Releasing };

  struct StereoSample {
    float left = 0.0f;
    float right = 0.0f;
  };

  // Core note state
  std::atomic<State> state_{State::Free};
  const SampleZone *activeZone_{nullptr};
  uint8_t currentPitch_{0};
  float currentVelocity_{0.0f};
  int currentStringId_{-1};

  // Sampler Playhead Time Model
  double directReadPos_{0.0};
  double pitchRatio_{1.0};

  // Envelope / Release
  float envelopeLevel_{0.0f};
  double releasePos_{0.0};
  
  // A slightly longer release tail (150ms) to simulate natural string damping 
  // when a real pick strikes a ringing string, rather than an abrupt electronic cut.
  double releaseSamples_{44100.0 * 0.15}; 
  float releaseStartAmp_{1.0f};

  // The only DSP math we need now: smooth fractional playhead reading
  StereoSample ReadInterpolated(double readPos) const;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix