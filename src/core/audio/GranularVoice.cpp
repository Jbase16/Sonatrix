#include "GranularVoice.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Sonatrix {
namespace Core {
namespace Audio {

namespace {

inline double Clamp(double x, double lo, double hi) {
  return std::max(lo, std::min(x, hi));
}

inline float ClampFloat(float x, float lo, float hi) {
  return std::max(lo, std::min(x, hi));
}

// Hann window over [0, N)
inline float Hann(double age, double duration) {
  if (duration <= 1.0)
    return 1.0f;
  const double phase = (2.0 * M_PI * age) / duration;
  return static_cast<float>(0.5 * (1.0 - std::cos(phase)));
}

// Lock-free Xorshift32 returning a float between -1.0 and 1.0
inline float FastRandomFloat(uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return 2.0f * (static_cast<float>(state) / 4294967295.0f) - 1.0f;
}

} // namespace

void GranularVoice::ResetGrains() {
  for (auto &g : grains_) {
    g.internalReadPos = 0.0;
    g.durationSamples = 0.0;
    g.ageSamples = 0.0;
    g.active = false;
  }
}

bool GranularVoice::HasActiveGrains() const {
  for (const auto &g : grains_) {
    if (g.active)
      return true;
  }
  return false;
}

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

  masterTimePos_ = 0.0;
  directReadPos_ = 0.0;
  granularMacroPos_ = 0.0;
  timeAdvanceRate_ = 1.0;

  envelopeLevel_ = currentVelocity_;
  releasePos_ = 0.0;
  releaseStartAmp_ = envelopeLevel_;

  // Hybrid strategy: Preserve physical attack, crossfade to lush granular sustain
  attackBypassSamples_ = 0.045 * zone->sampleRate;
  transitionSamples_ = 0.018 * zone->sampleRate;
  sustainSeeded_ = false;

  // 1. LUSH & MATHEMATICALLY RIGID Grain Configuration
  // 60ms captures low-frequency cycles smoothly. 
  nominalGrainDurationSamples_ = 0.060 * zone->sampleRate; 
  
  // 15ms hop guarantees exactly 4 overlapping windows at all times.
  hopSizeSamples_ = 0.015 * zone->sampleRate; 
  
  grainReadSpeed_ = pitchRatio_;
  samplesUntilNextGrain_ = 0.0;

  ResetGrains();
  state_ = State::Active;
}

void GranularVoice::Stop() {
  if (state_ == State::Active) {
    state_ = State::Releasing;
    releasePos_ = 0.0;
    releaseStartAmp_ = envelopeLevel_;
  }
}

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

void GranularVoice::SpawnGrain(size_t maxSourceFrames) {
  if (!activeZone_ || maxSourceFrames < 2)
    return;

  Grain *slot = nullptr;
  for (auto &g : grains_) {
    if (!g.active) {
      slot = &g;
      break;
    }
  }
  if (!slot)
    return;

  // STRICT JITTER RULE: Only jitter the position, never the duration.
  // +/- 2ms is enough to break phase-lock (comb filtering) without destroying transients.
  float posJitterSamples =
      FastRandomFloat(prngState_) * 0.002f * activeZone_->sampleRate;
  const double sourceStart = Clamp(granularMacroPos_ + posJitterSamples, 0.0,
                                   static_cast<double>(maxSourceFrames - 2));

  slot->active = true;
  slot->internalReadPos = sourceStart;
  slot->ageSamples = 0.0;
  
  // Duration is locked strictly to 60ms to guarantee constant overlap math.
  slot->durationSamples = nominalGrainDurationSamples_;
}

GranularVoice::StereoSample
GranularVoice::RenderGranularSample(size_t maxSourceFrames) {
  StereoSample mix{};

  for (auto &g : grains_) {
    if (!g.active)
      continue;

    if (g.ageSamples >= g.durationSamples ||
        g.internalReadPos >= static_cast<double>(maxSourceFrames - 1)) {
      g.active = false;
      continue;
    }

    const float w = Hann(g.ageSamples, g.durationSamples);
    const StereoSample s = ReadInterpolated(g.internalReadPos);

    mix.left += s.left * w;
    mix.right += s.right * w;

    g.internalReadPos += grainReadSpeed_;
    g.ageSamples += 1.0;

    if (g.ageSamples >= g.durationSamples ||
        g.internalReadPos >= static_cast<double>(maxSourceFrames - 1)) {
      g.active = false;
    }
  }

  return mix;
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

    // Release envelope
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

    const double crossStart =
        std::max(0.0, attackBypassSamples_ - 0.5 * transitionSamples_);
    const double crossEnd = attackBypassSamples_ + 0.5 * transitionSamples_;

    if (!sustainSeeded_ && masterTimePos_ >= crossStart) {
      sustainSeeded_ = true;
      samplesUntilNextGrain_ = 0.0;
      granularMacroPos_ = directReadPos_;
    }

    // Direct attack path
    StereoSample direct{};
    if (masterTimePos_ <= crossEnd &&
        directReadPos_ < static_cast<double>(maxSourceFrames - 1)) {
      direct = ReadInterpolated(directReadPos_);
      directReadPos_ += pitchRatio_;
    }

    // Sustain grain scheduler
    if (sustainSeeded_ &&
        granularMacroPos_ < static_cast<double>(maxSourceFrames - 1)) {
      while (samplesUntilNextGrain_ <= 0.0) {
        SpawnGrain(maxSourceFrames);
        
        // NO HOP JITTER ALLOWED. We enforce mathematical alignment.
        samplesUntilNextGrain_ += hopSizeSamples_;
      }
      samplesUntilNextGrain_ -= 1.0;
    }

    StereoSample granular = RenderGranularSample(maxSourceFrames);

    // STATIC COMPENSATION. 
    // A constant overlap factor of K Hann windows mathematically sums to exactly K/2.
    // We have a 60ms duration and 15ms hop, which is K = 4 overlapping windows.
    // Therefore, they sum perfectly to 2.0. We multiply by 0.5 to normalize.
    const float kConstantOverlapGain = 0.5f;
    granular.left *= kConstantOverlapGain;
    granular.right *= kConstantOverlapGain;

    // Equal-power crossfade
    float directWeight = 0.0f;
    float granularWeight = 0.0f;

    if (masterTimePos_ < crossStart) {
      directWeight = 1.0f;
      granularWeight = 0.0f;
    } else if (masterTimePos_ > crossEnd) {
      directWeight = 0.0f;
      granularWeight = 1.0f;
    } else {
      const double t = (masterTimePos_ - crossStart) /
                       std::max(1.0, (crossEnd - crossStart));
      const double a = Clamp(t, 0.0, 1.0);
      directWeight = static_cast<float>(std::cos(a * (M_PI * 0.5)));
      granularWeight = static_cast<float>(std::sin(a * (M_PI * 0.5)));
    }

    const float outL =
        (direct.left * directWeight + granular.left * granularWeight) *
        envelopeLevel_;
    const float outR =
        (direct.right * directWeight + granular.right * granularWeight) *
        envelopeLevel_;

    if (numChannels >= 1)
      outputChannels[0][f] += outL;
    if (numChannels >= 2)
      outputChannels[1][f] += outR;

    // Macro sustain timeline remains natural-speed
    if (masterTimePos_ < static_cast<double>(maxSourceFrames - 1)) {
      masterTimePos_ += 1.0;
    }

    if (sustainSeeded_ &&
        granularMacroPos_ < static_cast<double>(maxSourceFrames - 1)) {
      granularMacroPos_ += timeAdvanceRate_;
    }

    const bool sourceDone =
        masterTimePos_ >= static_cast<double>(maxSourceFrames - 1);
    const bool directDone =
        directReadPos_ >= static_cast<double>(maxSourceFrames - 1);
    const bool grainsDone = !HasActiveGrains();

    if (sourceDone && directDone && grainsDone) {
      state_ = State::Free;
      return;
    }
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix