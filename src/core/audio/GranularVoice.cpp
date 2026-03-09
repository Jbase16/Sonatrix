#include "GranularVoice.h"

#include <algorithm>
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace Audio {

namespace {

inline float ClampFloat(float x, float lo, float hi) {
  return std::max(lo, std::min(x, hi));
}

} // namespace

void GranularVoice::Start(const SampleZone *zone, uint8_t targetPitch,
                          double pitchRatio, float velocity) {
  if (!zone || zone->audioData.empty() || zone->numChannels == 0 ||
      zone->sampleRate <= 0.0) {
    state_ = State::Free;
    activeZone_ = nullptr;
    return;
  }

  activeZone_ = zone;
  currentPitch_ = targetPitch;
  pitchRatio_ = std::max(0.01, pitchRatio);
  currentVelocity_ = ClampFloat(velocity, 0.0f, 1.0f);

  // Sampler logic only needs ONE playhead
  directReadPos_ = 0.0;
  
  envelopeLevel_ = currentVelocity_;
  releasePos_ = 0.0;
  releaseStartAmp_ = envelopeLevel_;

  state_ = State::Active;
}

void GranularVoice::Stop() {
  if (state_ == State::Active) {
    state_ = State::Releasing;
    releasePos_ = 0.0;
    releaseStartAmp_ = envelopeLevel_;
  }
}

// This is the core "Sampler" function. It reads fractional samples to pitch shift smoothly.
GranularVoice::StereoSample
GranularVoice::ReadInterpolated(double readPos) const {
  StereoSample out{};

  if (!activeZone_ || activeZone_->audioData.empty() ||
      activeZone_->numChannels == 0)
    return out;

  const auto &data = activeZone_->audioData;
  const size_t channels = activeZone_->numChannels;
  const size_t maxFrames = data.size() / channels;

  if (readPos < 0.0 || readPos >= static_cast<double>(maxFrames - 1))
    return out;

  const size_t idx = static_cast<size_t>(readPos);
  const float frac = static_cast<float>(readPos - static_cast<double>(idx));

  const float l1 = data[idx * channels];
  const float l2 = data[(idx + 1) * channels];
  out.left = l1 + (l2 - l1) * frac;

  if (channels > 1) {
    const float r1 = data[idx * channels + 1];
    const float r2 = data[(idx + 1) * channels + 1];
    out.right = r1 + (r2 - r1) * frac;
  } else {
    out.right = out.left;
  }

  return out;
}

void GranularVoice::RenderNextBlock(float **outputChannels, uint32_t numFrames,
                                    uint32_t numChannels) {
  if (state_ == State::Free || !activeZone_ || !outputChannels ||
      numChannels == 0)
    return;

  const size_t maxSourceFrames =
      activeZone_->audioData.size() / activeZone_->numChannels;

  if (maxSourceFrames < 2) {
    state_ = State::Free;
    return;
  }

  for (uint32_t f = 0; f < numFrames; ++f) {
    if (state_ == State::Free)
      return;

    // 1. Release envelope
    if (state_ == State::Releasing) {
      const float rel =
          1.0f - static_cast<float>(releasePos_ / releaseSamples_);
      if (rel <= 0.0f) {
        state_ = State::Free;
        envelopeLevel_ = 0.0f;
        return;
      }
      envelopeLevel_ = rel * releaseStartAmp_;
      releasePos_ += 1.0;
    }

    // 2. PURE DIRECT PLAYBACK: No grains, no overlap math.
    StereoSample direct = ReadInterpolated(directReadPos_);

    // 3. Apply the envelope and write to the output buffers
    const float outL = direct.left * envelopeLevel_;
    const float outR = direct.right * envelopeLevel_;

    if (numChannels >= 1)
      outputChannels[0][f] += outL;
    if (numChannels >= 2)
      outputChannels[1][f] += outR;

    // 4. Advance the playhead by the exact pitch ratio (THIS is the sampler logic)
    directReadPos_ += pitchRatio_;

    // 5. Stop the voice when we hit the end of the physical sample
    if (directReadPos_ >= static_cast<double>(maxSourceFrames - 1)) {
      state_ = State::Free;
      return;
    }
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix