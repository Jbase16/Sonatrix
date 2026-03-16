#include "BassVoiceManager.h"

#include "AssetManager.h"

namespace Sonatrix {
namespace Core {
namespace Audio {

bool BassVoiceManager::LoadElectricBassKit(
    const std::string &assetsAbsolutePath) {
  auto &assetManager = AssetManager::GetInstance();
  if (!assetManager.LoadElectricBassAnchors(assetsAbsolutePath)) {
    return false;
  }

  return LoadArticulation(assetManager.GetElectricBassArticulation(),
                          MixerBus::Bass);
}

bool BassVoiceManager::LoadMockBassKit(const std::string &assetsAbsolutePath) {
  auto &assetManager = AssetManager::GetInstance();
  if (!assetManager.LoadMockBassAnchors(assetsAbsolutePath)) {
    return false;
  }

  return LoadArticulation(assetManager.GetMockBassArticulation(),
                          MixerBus::Bass);
}

void BassVoiceManager::ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) {
  for (const auto &event : events) {
    if (event.type == MIDI::MIDIEventType::NoteOn && event.data2 > 0) {
      StartVoiceForEvent(event);
      continue;
    }

    if (event.type == MIDI::MIDIEventType::NoteOff ||
        (event.type == MIDI::MIDIEventType::NoteOn && event.data2 == 0)) {
      StopVoiceForEvent(event);
    }
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
