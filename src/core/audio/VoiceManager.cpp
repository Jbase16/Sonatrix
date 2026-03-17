#include "VoiceManager.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace Sonatrix {
namespace Core {
namespace Audio {

bool VoiceManager::LoadArticulation(const InstrumentArticulation &articulation,
                                    MixerBus mixerBus) {
  ResetRuntimeState();
  activeArticulation_ = articulation;
  articulationOutputGain_ =
      (articulation.outputGain > 0.0f) ? articulation.outputGain : 1.0f;
  activeMixerBus_ = mixerBus;
  return HasLoadedArticulation();
}

bool VoiceManager::StartVoiceForEvent(const MIDI::MIDIEvent &event,
                                      int stringId) {
  if (!HasLoadedArticulation() || event.data2 == 0) {
    return false;
  }

  const SampleZone *zone =
      activeArticulation_.FindZone(event.data1, event.data2, stringId);
  if (!zone) {
    return false;
  }

  SamplerVoice *voice = GetBestAvailableVoice();
  if (!voice) {
    return false;
  }

  const double pitchRatio = std::pow(
      2.0, (static_cast<double>(event.data1) - zone->rootKey) / 12.0);
  const float velocity = static_cast<float>(event.data2) / 127.0f;
  voice->Start(zone, event.data1, pitchRatio, velocity, stringId);
  return true;
}

void VoiceManager::StopVoiceForEvent(const MIDI::MIDIEvent &event,
                                     int stringId) {
  SamplerVoice *oldestActiveMatch = nullptr;
  SamplerVoice *oldestReleasingMatch = nullptr;
  uint64_t oldestActiveSequence = std::numeric_limits<uint64_t>::max();
  uint64_t oldestReleasingSequence = std::numeric_limits<uint64_t>::max();

  for (auto &voice : voices_) {
    if (voice.IsFree() || voice.GetCurrentPitch() != event.data1) {
      continue;
    }

    if (stringId >= 0 && voice.GetStringId() != stringId) {
      continue;
    }

    if (voice.IsReleasing()) {
      if (voice.GetStartSequence() < oldestReleasingSequence) {
        oldestReleasingSequence = voice.GetStartSequence();
        oldestReleasingMatch = &voice;
      }
      continue;
    }

    if (voice.GetStartSequence() < oldestActiveSequence) {
      oldestActiveSequence = voice.GetStartSequence();
      oldestActiveMatch = &voice;
    }
  }

  if (oldestActiveMatch) {
    oldestActiveMatch->Stop();
    return;
  }

  if (oldestReleasingMatch) {
    oldestReleasingMatch->Stop();
  }
}

void VoiceManager::ChokeVoicesOnString(int stringId) {
  if (stringId < 0) {
    return;
  }

  for (auto &voice : voices_) {
    if (!voice.IsFree() && voice.GetStringId() == stringId) {
      voice.Choke();
    }
  }
}

void VoiceManager::ResetRuntimeState() {
  for (auto &voice : voices_) {
    voice.Reset();
  }
}

SamplerVoice *VoiceManager::GetBestAvailableVoice() {
  SamplerVoice *worstVoice = nullptr;
  float lowestPriority = std::numeric_limits<float>::max();

  for (auto &voice : voices_) {
    if (voice.IsFree()) {
      return &voice;
    }

    const float priority = voice.GetStealingPriority();
    if (priority < lowestPriority) {
      lowestPriority = priority;
      worstVoice = &voice;
    }
  }

  return worstVoice;
}

void VoiceManager::RenderAudio(float **outputChannels, uint32_t numFrames,
                               uint32_t numChannels) {
  AudioMixer::ClearBuffers(outputChannels, numFrames, numChannels);

  static thread_local std::vector<float> tempL;
  static thread_local std::vector<float> tempR;

  if (tempL.size() < numFrames) {
    tempL.resize(numFrames);
  }
  if (tempR.size() < numFrames) {
    tempR.resize(numFrames);
  }

  std::memset(tempL.data(), 0, numFrames * sizeof(float));
  std::memset(tempR.data(), 0, numFrames * sizeof(float));

  float *tempChannels[2] = {tempL.data(), tempR.data()};

  for (auto &voice : voices_) {
    voice.RenderNextBlock(tempChannels, numFrames, 2);
  }

  if (articulationOutputGain_ != 1.0f) {
    for (uint32_t i = 0; i < numFrames; ++i) {
      tempL[i] *= articulationOutputGain_;
      tempR[i] *= articulationOutputGain_;
    }
  }

  mixer_.MixBusToOutput(activeMixerBus_, tempChannels, outputChannels, numFrames,
                        numChannels);
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
