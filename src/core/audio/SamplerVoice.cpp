#include "SamplerVoice.h"

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

void SamplerVoice::Start(const SampleZone *zone, uint8_t targetPitch,
                         double pitchRatio, float velocity, int stringId) {
  if (!zone || zone->audioData.empty() || zone->numChannels == 0 ||
      zone->sampleRate <= 0.0) {
    state_ = State::Free;
    activeZone_ = nullptr;
    currentPitch_ = 0;
    currentStringId_ = -1;
    currentVelocity_ = 0.0f;
    envelopeLevel_ = 0.0f;
    return;
  }

  activeZone_ = zone;
  currentPitch_ = targetPitch;
  currentStringId_ = stringId;
  pitchRatio_ = std::max(0.01, pitchRatio);
  currentVelocity_ = ClampFloat(velocity, 0.0f, 1.0f);

  // Reset sampler playhead
  directReadPos_ = 0.0;

  // Reset attack / release state
  attackPos_ = 0.0;
  envelopeLevel_ = 0.0f;
  releasePos_ = 0.0;
  releaseStartAmp_ = currentVelocity_;
  activeReleaseSamples_ = releaseSamples_;

  state_ = State::Active;
}

void SamplerVoice::Stop() {
  if (state_ == State::Active) {
    state_ = State::Releasing;
    releasePos_ = 0.0;
    releaseStartAmp_ = envelopeLevel_;
    activeReleaseSamples_ = releaseSamples_;
  }
}

void SamplerVoice::Choke() {
  if (state_ == State::Active || state_ == State::Releasing) {
    state_ = State::Releasing;
    releasePos_ = 0.0;
    releaseStartAmp_ = envelopeLevel_;
    activeReleaseSamples_ = chokeReleaseSamples_;
  }
}

SamplerVoice::StereoSample
SamplerVoice::ReadInterpolated(double readPos) const {
  StereoSample out{};

  if (!activeZone_ || activeZone_->audioData.empty() ||
      activeZone_->numChannels == 0) {
    return out;
  }

  const auto &data = activeZone_->audioData;
  const size_t channels = activeZone_->numChannels;
  const size_t maxFrames = data.size() / channels;

  if (maxFrames < 2) {
    return out;
  }

  if (readPos < 0.0 || readPos >= static_cast<double>(maxFrames - 1)) {
    return out;
  }

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

void SamplerVoice::RenderNextBlock(float **outputChannels, uint32_t numFrames,
                                   uint32_t numChannels) {
  if (state_ == State::Free || !activeZone_ || !outputChannels ||
      numChannels == 0) {
    return;
  }

  const size_t maxSourceFrames =
      activeZone_->audioData.size() / activeZone_->numChannels;

  if (maxSourceFrames < 2) {
    state_ = State::Free;
    envelopeLevel_ = 0.0f;
    return;
  }

  for (uint32_t f = 0; f < numFrames; ++f) {
    if (state_ == State::Free) {
      return;
    }

    // Envelope handling
    if (state_ == State::Active) {
      if (attackPos_ < attackSamples_) {
        const float attackGain =
            static_cast<float>(attackPos_ / attackSamples_);
        envelopeLevel_ = currentVelocity_ * attackGain;
        attackPos_ += 1.0;
      } else {
        envelopeLevel_ = currentVelocity_;
      }
    } else if (state_ == State::Releasing) {
      if (activeReleaseSamples_ <= 1.0) {
        state_ = State::Free;
        envelopeLevel_ = 0.0f;
        return;
      }

      const float rel =
          1.0f - static_cast<float>(releasePos_ / activeReleaseSamples_);
      if (rel <= 0.0f) {
        state_ = State::Free;
        envelopeLevel_ = 0.0f;
        return;
      }

      envelopeLevel_ = (rel * rel) * releaseStartAmp_;
      releasePos_ += 1.0;
    }

    StereoSample direct = ReadInterpolated(directReadPos_);

    const float outL = direct.left * envelopeLevel_;
    const float outR = direct.right * envelopeLevel_;

    if (numChannels >= 1) {
      outputChannels[0][f] += outL;
    }
    if (numChannels >= 2) {
      outputChannels[1][f] += outR;
    }

    directReadPos_ += pitchRatio_;

    if (directReadPos_ >= static_cast<double>(maxSourceFrames - 1)) {
      state_ = State::Free;
      envelopeLevel_ = 0.0f;
      return;
    }
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix