#include "VoiceManager.h"
#include "AudioFileReader.h"
#include <limits>

namespace Sonatrix {
namespace Core {
namespace Audio {

void VoiceManager::ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events,
                               InstrumentArticulation &articulation) {
  for (const auto &ev : events) {
    if (ev.type == MIDI::MIDIEventType::NoteOn && ev.data2 > 0) {

      // 1. Determine which physical acoustic zone to load based on the sparse
      // matrix
      const SampleZone *zone = articulation.FindZone(ev.data1, ev.data2);
      if (!zone)
        continue; // No matching sample found

      // 2. Allocate a voice (stealing if necessary)
      AudioVoice *v = GetBestAvailableVoice();
      if (v) {
        // If it was stolen while sustaining, forcefully stop it first (to avoid
        // clicks ideally we'd do a 5ms fast-fade, but we mock the immediate
        // override here).
        v->Start(zone, ev.data1, ev.data2);
      }

    } else if (ev.type == MIDI::MIDIEventType::NoteOff ||
               (ev.type == MIDI::MIDIEventType::NoteOn && ev.data2 == 0)) {
      // Find the active voice playing this pitch and release it.
      // Temporary Phase 6 hack: The real engine uses the Constraint Solver
      // downstream for voice killing, but we need manual NoteOff to stop
      // indefinite synth swelling during hardware tests.
      for (auto &v : voices_) {
        if (!v.IsFree() && v.GetCurrentPitch() == ev.data1) {
          v.Stop();
        }
      }
    }
  }
}

AudioVoice *VoiceManager::GetBestAvailableVoice() {
  AudioVoice *worstVoice = nullptr;
  float lowestPriority = std::numeric_limits<float>::max();

  for (auto &v : voices_) {
    if (v.IsFree()) {
      return &v; // Immediate success
    }

    float p = v.GetStealingPriority();
    if (p < lowestPriority) {
      lowestPriority = p;
      worstVoice = &v;
    }
  }

  // Voice Stealing occurred. We return the active voice with the lowest
  // volume/envelope priority.
  return worstVoice;
}

void VoiceManager::LoadInstrumentKit(const std::string &assetsAbsolutePath) {
  activeArticulation_.name = "Bass_Sawtooth_Mock";
  activeArticulation_.zones.clear();

  // Load C1 (MIDI 36)
  SampleZone zoneC1;
  zoneC1.filePath = assetsAbsolutePath + "/C1.wav";
  zoneC1.rootKey = 36;
  zoneC1.lowVelocity = 0;
  zoneC1.highVelocity = 127;
  zoneC1.isLoaded = AudioFileReader::LoadFile(zoneC1.filePath, zoneC1.audioData,
                                              zoneC1.sampleRate);
  activeArticulation_.zones.push_back(zoneC1);

  // Load C2 (MIDI 48)
  SampleZone zoneC2;
  zoneC2.filePath = assetsAbsolutePath + "/C2.wav";
  zoneC2.rootKey = 48;
  zoneC2.lowVelocity = 0;
  zoneC2.highVelocity = 127;
  zoneC2.isLoaded = AudioFileReader::LoadFile(zoneC2.filePath, zoneC2.audioData,
                                              zoneC2.sampleRate);
  activeArticulation_.zones.push_back(zoneC2);

  // Load C3 (MIDI 60)
  SampleZone zoneC3;
  zoneC3.filePath = assetsAbsolutePath + "/C3.wav";
  zoneC3.rootKey = 60;
  zoneC3.lowVelocity = 0;
  zoneC3.highVelocity = 127;
  zoneC3.isLoaded = AudioFileReader::LoadFile(zoneC3.filePath, zoneC3.audioData,
                                              zoneC3.sampleRate);
  activeArticulation_.zones.push_back(zoneC3);
}

void VoiceManager::RenderAudio(float **outputChannels, uint32_t numFrames,
                               uint32_t numChannels) {
  // Process every voice and accumulate into the final output buffer
  for (auto &v : voices_) {
    v.RenderNextBlock(outputChannels, numFrames, numChannels);
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
