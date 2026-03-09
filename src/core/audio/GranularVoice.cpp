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

// Tiny deterministic PRNG so grains are decorrelated without pulling in
// <random>.
double GranularVoice::NextRand01() {
  grainCounter_ = grainCounter_ * 1664525u + 1013904223u;
  return static_cast<double>(grainCounter_) / static_cast<double>(UINT32_MAX);
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
  // Speed up the macro envelope subtly for higher pitches so decay feels
  // natural
  timeAdvanceRate_ = std::max(1.0, std::sqrt(pitchRatio_));
  granularMacroPos_ = 0.0;

  envelopeLevel_ = currentVelocity_;
  releasePos_ = 0.0;
  releaseStartAmp_ = envelopeLevel_;

  // For plucked / struck guitar-like sources:
  // keep the first ~45 ms as direct resampled playback,
  // then crossfade ~18 ms into granular sustain.
  attackBypassSamples_ = 0.045 * zone->sampleRate;
  transitionSamples_ = 0.018 * zone->sampleRate;
  sustainSeeded_ = false;

  // PSOLA Mapping
  double rootFreq = 440.0 * std::pow(2.0, (zone->rootKey - 69) / 12.0);
  double targetFreq = 440.0 * std::pow(2.0, (targetPitch - 69) / 12.0);

  rootPeriodSamples_ = zone->sampleRate / rootFreq;
  targetPeriodSamples_ = zone->sampleRate / targetFreq;

  // Grain size is perfectly 2 periods of the original sound for clean crossfade
  nominalGrainDurationSamples_ = rootPeriodSamples_ * 2.0;

  // Hop size is exactly 1 period of the TARGET pitch
  hopSizeSamples_ = targetPeriodSamples_;

  // To preserve formants (body resonance), we read the source at 1.0x inside
  // the grain
  grainReadSpeed_ = 1.0;

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

  if (maxFrames == 0)
    return out;

  if (readPos < 0.0)
    readPos = 0.0;

  size_t idx = static_cast<size_t>(readPos);
  if (idx >= maxFrames) {
    idx = maxFrames - 1;
  }

  const size_t nextIdx = std::min(idx + 1, maxFrames - 1);
  const float frac = static_cast<float>(readPos - static_cast<double>(idx));

  const float l1 = data[idx * channels];
  const float l2 = data[nextIdx * channels];
  out.left = l1 + (l2 - l1) * frac;

  if (channels > 1) {
    const float r1 = data[idx * channels + 1];
    const float r2 = data[nextIdx * channels + 1];
    out.right = r1 + (r2 - r1) * frac;
  } else {
    out.right = out.left;
  }

  return out;
}

void GranularVoice::SpawnGrain(size_t maxSourceFrames) {
  if (!activeZone_ || maxSourceFrames < 2)
    return;

  // Find a free grain slot
  Grain *slot = nullptr;
  for (auto &g : grains_) {
    if (!g.active) {
      slot = &g;
      break;
    }
  }
  if (!slot)
    return;

  // PSOLA Phase Lock:
  // We snap the source starting position to the nearest original pitch epoch
  // relative to the attack bypass. This guarantees that all grains start at the
  // exact same relative phase, completely eliminating granular comb-filtering /
  // reverb.
  double offset = granularMacroPos_ - attackBypassSamples_;
  double periods = std::round(offset / rootPeriodSamples_);
  double sourceStart = attackBypassSamples_ + periods * rootPeriodSamples_;

  sourceStart = Clamp(sourceStart, attackBypassSamples_,
                      static_cast<double>(maxSourceFrames - 2));

  slot->active = true;
  slot->internalReadPos = sourceStart;
  slot->ageSamples = 0.0;
  slot->durationSamples = std::max(8.0, nominalGrainDurationSamples_);
}

GranularVoice::StereoSample
GranularVoice::RenderGranularSample(size_t maxSourceFrames, float &outNorm) {
  StereoSample mix{};
  outNorm = 0.0f;

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
    outNorm += w;

    // Read at exactly 1.0x to preserve acoustic formants and phase
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

  const auto &data = activeZone_->audioData;
  const size_t maxSourceFrames = data.size() / activeZone_->numChannels;
  if (maxSourceFrames < 2) {
    state_ = State::Free;
    return;
  }

  for (uint32_t f = 0; f < numFrames; ++f) {
    if (state_ == State::Free)
      return;

    // Envelope / release
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

    // Figure out transition region:
    // attack direct-only -> attack/sustain crossfade -> sustain granular-only
    const double crossStart =
        std::max(0.0, attackBypassSamples_ - 0.5 * transitionSamples_);
    const double crossEnd = attackBypassSamples_ + 0.5 * transitionSamples_;

    // Start seeding grains slightly before the crossfade so the sustain engine
    // is already alive when the direct attack fades out.
    if (!sustainSeeded_ && masterTimePos_ >= crossStart) {
      sustainSeeded_ = true;
      samplesUntilNextGrain_ = 0.0;
      granularMacroPos_ = directReadPos_; // Sync granular engine to the exact
                                          // position the attack reached!
    }

    // Direct attack path (pitched via direct resampling, not granularized)
    StereoSample direct{};
    if (masterTimePos_ <= crossEnd &&
        directReadPos_ < static_cast<double>(maxSourceFrames - 1)) {
      direct = ReadInterpolated(directReadPos_);
      directReadPos_ += pitchRatio_;
    }

    // Grain scheduler for sustain path
    if (sustainSeeded_ &&
        granularMacroPos_ < static_cast<double>(maxSourceFrames - 1)) {
      while (samplesUntilNextGrain_ <= 0.0) {
        SpawnGrain(maxSourceFrames);
        samplesUntilNextGrain_ += hopSizeSamples_;
      }
      samplesUntilNextGrain_ -= 1.0;
    }

    // Render granular sustain path
    float norm = 0.0f;
    StereoSample granular = RenderGranularSample(maxSourceFrames, norm);
    if (norm > 1e-6f) {
      granular.left /= norm;
      granular.right /= norm;
    }

    // Crossfade direct -> granular
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
      // Equal-power style crossfade
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

    // Sustain timeline always advances at 1.0x
    if (masterTimePos_ < static_cast<double>(maxSourceFrames - 1)) {
      masterTimePos_ += 1.0;
    }

    // Macro timeline advances independently
    if (sustainSeeded_ &&
        granularMacroPos_ < static_cast<double>(maxSourceFrames - 1)) {
      granularMacroPos_ += timeAdvanceRate_;
    }

    // Once the source timeline is exhausted, stop spawning and let active
    // grains drain.
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