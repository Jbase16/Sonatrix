#include "VoiceManager.h"
#include "AssetManager.h"
#include "AudioFileReader.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace Sonatrix {
namespace Core {
namespace Audio {

namespace {

bool FileExists(const std::string &path) {
  std::ifstream file(path);
  return file.good();
}

bool LoadZoneIfPresent(const std::string &directoryPath, const char *fileName,
                       uint8_t rootKey,
                       InstrumentArticulation &articulation) {
  const std::string filePath = directoryPath + "/" + fileName;
  if (!FileExists(filePath)) {
    return false;
  }

  SampleZone zone;
  zone.filePath = filePath;
  zone.rootKey = rootKey;
  zone.lowVelocity = 0;
  zone.highVelocity = 127;
  zone.isLoaded =
      AudioFileReader::LoadFile(zone.filePath, zone.audioData, zone.sampleRate);
  if (!zone.isLoaded) {
    return false;
  }

  articulation.zones.push_back(std::move(zone));
  return true;
}

bool LooksLikeAcousticGuitarKit(const std::string &directoryPath) {
  static constexpr const char *kRequiredFiles[] = {
      "E2.wav", "A2.wav", "D3.wav", "G3.wav", "B3.wav", "E4.wav"};

  for (const char *fileName : kRequiredFiles) {
    if (!FileExists(directoryPath + "/" + fileName)) {
      return false;
    }
  }

  return true;
}

bool LooksLikeElectricBassKit(const std::string &directoryPath) {
  static constexpr const char *kRequiredFiles[] = {"B1.wav", "C3.wav", "C4.wav"};

  for (const char *fileName : kRequiredFiles) {
    if (!FileExists(directoryPath + "/" + fileName)) {
      return false;
    }
  }

  return true;
}

} // namespace

void VoiceManager::ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events,
                               const InstrumentArticulation &articulation) {
  for (const auto &ev : events) {
    const int stringId =
        (ev.channel >= 1 && ev.channel <= 6) ? static_cast<int>(ev.channel) - 1
                                             : -1;

    if (ev.type == MIDI::MIDIEventType::NoteOn && ev.data2 > 0) {
      // Physical string restrike handling:
      // choke any existing voice on the same physical string quickly, instead
      // of letting it linger like a sad ghost.
      if (stringId >= 0 && stringId < 6) {
        stringActiveNotes_[stringId]++;

        for (auto &v : voices_) {
          if (!v.IsFree() && v.GetStringId() == stringId) {
            v.Choke();
          }
        }
      }

      const SampleZone *zone = articulation.FindZone(ev.data1, ev.data2, stringId);
      if (!zone) {
        continue;
      }

      SamplerVoice *v = GetBestAvailableVoice();
      if (v) {
        const double pitchRatio =
            std::pow(2.0, (static_cast<double>(ev.data1) - zone->rootKey) / 12.0);
        const float velocity = ev.data2 / 127.0f;
        v->Start(zone, ev.data1, pitchRatio, velocity, stringId);
      }

    } else if (ev.type == MIDI::MIDIEventType::NoteOff ||
               (ev.type == MIDI::MIDIEventType::NoteOn && ev.data2 == 0)) {
      // Strict NoteOff matching with orphan protection for same-string restrikes.
      if (stringId >= 0 && stringId < 6) {
        stringActiveNotes_[stringId]--;

        if (stringActiveNotes_[stringId] > 0) {
          // A newer strike is already active on this string.
          continue;
        }

        if (stringActiveNotes_[stringId] < 0) {
          stringActiveNotes_[stringId] = 0;
        }
      }

      for (auto &v : voices_) {
        if (!v.IsFree() && v.GetCurrentPitch() == ev.data1 &&
            v.GetStringId() == stringId) {
          v.Stop();
          break;
        }
      }
    }
  }
}

SamplerVoice *VoiceManager::GetBestAvailableVoice() {
  SamplerVoice *worstVoice = nullptr;
  float lowestPriority = std::numeric_limits<float>::max();

  for (auto &v : voices_) {
    if (v.IsFree()) {
      return &v;
    }

    const float p = v.GetStealingPriority();
    if (p < lowestPriority) {
      lowestPriority = p;
      worstVoice = &v;
    }
  }

  return worstVoice;
}

void VoiceManager::LoadInstrumentKit(const std::string &assetsAbsolutePath) {
  activeArticulation_.zones.clear();
  activeArticulation_.name.clear();
  activeMixerBus_ = MixerBus::Bass;

  if (LooksLikeAcousticGuitarKit(assetsAbsolutePath)) {
    auto &assetManager = AssetManager::GetInstance();
    if (assetManager.LoadAcousticGuitarAnchors(assetsAbsolutePath)) {
      activeArticulation_ = assetManager.GetAcousticGuitarArticulation();
      activeArticulation_.name = "Acoustic_Guitar_Anchors";
      activeMixerBus_ = MixerBus::Guitar;
      return;
    }
  }

  if (LooksLikeElectricBassKit(assetsAbsolutePath)) {
    auto &assetManager = AssetManager::GetInstance();
    if (assetManager.LoadElectricBassAnchors(assetsAbsolutePath)) {
      activeArticulation_ = assetManager.GetElectricBassArticulation();
      activeArticulation_.name = "Electric_Bass_Anchors";
      activeMixerBus_ = MixerBus::Bass;
      return;
    }
  }

  activeArticulation_.name = "Bass_Sawtooth_Mock";
  LoadZoneIfPresent(assetsAbsolutePath, "C1.wav", 36, activeArticulation_);
  LoadZoneIfPresent(assetsAbsolutePath, "C2.wav", 48, activeArticulation_);
  LoadZoneIfPresent(assetsAbsolutePath, "C3.wav", 60, activeArticulation_);
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

  for (auto &v : voices_) {
    v.RenderNextBlock(tempChannels, numFrames, 2);
  }

  mixer_.MixBusToOutput(activeMixerBus_, tempChannels, outputChannels, numFrames,
                        numChannels);
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix