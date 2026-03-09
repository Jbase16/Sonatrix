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
      GranularVoice *v = GetBestAvailableVoice();
      if (v) {
        // Calculate the pitch shift ratio in real-time
        double pitchRatio = std::pow(
            2.0, (static_cast<double>(ev.data1) - zone->rootKey) / 12.0);
        float velocity = ev.data2 / 127.0f;

        v->Start(zone, ev.data1, pitchRatio, velocity);
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

GranularVoice *VoiceManager::GetBestAvailableVoice() {
  GranularVoice *worstVoice = nullptr;
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

  // 1. Clear the master output buffers first, as the Mixer accumulates rather
  // than overwrites
  AudioMixer::ClearBuffers(outputChannels, numFrames, numChannels);

  // 2. We need a temporary buffer to render the voices into before hitting the
  // mixer. We use thread_local to avoid allocation on the realtime audio
  // thread. In a full implementation, each instrument engine (Drums, Bass, etc)
  // would manage its own VoiceManager / voices and render to its specific bus.
  // For Phase 12, since this monolithic VoiceManager holds the
  // "Bass_Sawtooth_Mock" kit, we will render all its voices into a single
  // temporary stereo buffer and route it to MixerBus::Bass.
  static thread_local std::vector<float> tempL;
  static thread_local std::vector<float> tempR;

  if (tempL.size() < numFrames)
    tempL.resize(numFrames);
  if (tempR.size() < numFrames)
    tempR.resize(numFrames);

  std::memset(tempL.data(), 0, numFrames * sizeof(float));
  std::memset(tempR.data(), 0, numFrames * sizeof(float));

  float *tempChannels[2] = {tempL.data(), tempR.data()};

  // 3. Process every voice and accumulate into the temporary bus buffer
  for (auto &v : voices_) {
    v.RenderNextBlock(tempChannels, numFrames, 2);
  }

  // 4. Send the accumulated Bass bus through the Mixer to apply Gain/Pan and
  // sum into the final master outputChannels.
  mixer_.MixBusToOutput(MixerBus::Bass, tempChannels, outputChannels, numFrames,
                        numChannels);
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
