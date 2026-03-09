#include "GranularVoice.h"

namespace Sonatrix {
namespace Core {
namespace Audio {

void GranularVoice::Start(const SampleZone *zone, uint8_t targetPitch,
                          double pitchRatio, float velocity) {
  if (!zone || zone->audioData.empty()) {
    state_ = State::Free;
    return;
  }

  activeZone_ = zone;
  currentPitch_ = targetPitch;
  pitchRatio_ = pitchRatio;
  currentVelocity_ = velocity;

  masterTimePos_ = 0.0;
  envelopeLevel_ = velocity; // Assuming instantaneous attack for plucked/struck
                             // acoustic instruments
  envelopeTarget_ = velocity;

  // 30ms grain size is standard for preserving transients without too much echo
  grainDurationSamples_ = 0.030 * zone->sampleRate;

  // 15ms hop size (50% overlap for Hanning window)
  hopSizeSamples_ = grainDurationSamples_ * 0.5;

  // Spawn the very first grain immediately
  samplesUntilNextGrain_ = 0.0;

  for (auto &g : grains_) {
    g.active = false;
  }

  state_ = State::Active;
}

void GranularVoice::Stop() {
  if (state_ == State::Active) {
    state_ = State::Releasing;
    releasePos_ = 0.0;
    releaseStartAmp_ = envelopeLevel_;
  }
}

void GranularVoice::RenderNextBlock(float **outputChannels, uint32_t numFrames,
                                    uint32_t numChannels) {
  if (state_ == State::Free || !activeZone_)
    return;

  const auto &data = activeZone_->audioData;
  const size_t maxSourceIndex =
      data.size() / activeZone_->numChannels; // total frames

  for (uint32_t f = 0; f < numFrames; ++f) {

    // 1. ADSR Envelope update
    if (state_ == State::Releasing) {
      float rel = 1.0f - static_cast<float>(releasePos_ / releaseSamples_);
      if (rel <= 0.0f) {
        state_ = State::Free;
        envelopeLevel_ = 0.0f;
        return; // Finished
      }
      envelopeLevel_ = rel * releaseStartAmp_;
      releasePos_ += 1.0;
    }

    // 2. Grain Spawning scheduler
    if (samplesUntilNextGrain_ <= 0.0) {
      // Find a free grain
      for (auto &g : grains_) {
        if (!g.active) {
          g.active = true;
          // The grain starts reading EXACTLY where the master time pointer is.
          g.internalReadPos = masterTimePos_;
          g.ageSamples = 0.0;
          g.durationSamples = grainDurationSamples_;
          break;
        }
      }
      samplesUntilNextGrain_ += hopSizeSamples_;
    }
    samplesUntilNextGrain_ -= 1.0;

    // 3. Render and overlapping grains
    float mixL = 0.0f;
    float mixR = 0.0f;

    for (auto &g : grains_) {
      if (!g.active)
        continue;

      // Hanning Window calculation
      // window(n) = 0.5 * (1 - cos((2 * PI * n) / N))
      double windowPhase = (2.0 * M_PI * g.ageSamples) / g.durationSamples;
      float windowAmp = 0.5f * (1.0f - std::cos(windowPhase));

      // Read from the sample zone using linear interpolation
      size_t idx = static_cast<size_t>(g.internalReadPos);
      if (idx >= maxSourceIndex) {
        // Grain hit the end of the file
        g.active = false;
        continue;
      }

      size_t nextIdx = std::min(idx + 1, maxSourceIndex - 1);
      float frac = static_cast<float>(g.internalReadPos - idx);

      // We support interleaved stereo audio in the SampleZone. If Mono,
      // duplicate to stereo.
      float sL1 = data[idx * activeZone_->numChannels];
      float sL2 = data[nextIdx * activeZone_->numChannels];
      float valL = sL1 + (sL2 - sL1) * frac;

      float sR1 = (activeZone_->numChannels > 1)
                      ? data[idx * activeZone_->numChannels + 1]
                      : sL1;
      float sR2 = (activeZone_->numChannels > 1)
                      ? data[nextIdx * activeZone_->numChannels + 1]
                      : sL2;
      float valR = sR1 + (sR2 - sR1) * frac;

      mixL += valL * windowAmp;
      mixR += valR * windowAmp;

      // Advance grain playhead at the PITCH SHIFT ratio
      g.internalReadPos += pitchRatio_;
      g.ageSamples += 1.0;

      if (g.ageSamples >= g.durationSamples) {
        g.active = false;
      }
    }

    // Advance master time pointer at strictly 1.0x speed to preserve original
    // length
    masterTimePos_ += 1.0;

    // Apply global envelope and write to output
    if (numChannels >= 1) {
      outputChannels[0][f] += mixL * envelopeLevel_;
    }
    if (numChannels >= 2) {
      outputChannels[1][f] += mixR * envelopeLevel_;
    }

    // Stop if the master time has exceeded the file length (we reached the
    // physical end)
    if (masterTimePos_ >= maxSourceIndex) {
      state_ = State::Free;
      return;
    }
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
