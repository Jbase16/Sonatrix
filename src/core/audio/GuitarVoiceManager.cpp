#include "GuitarVoiceManager.h"

#include "AssetManager.h"

namespace Sonatrix {
namespace Core {
namespace Audio {

bool GuitarVoiceManager::LoadAcousticGuitarKit(
    const std::string &assetsAbsolutePath) {
  auto &assetManager = AssetManager::GetInstance();
  if (!assetManager.LoadAcousticGuitarAnchors(assetsAbsolutePath)) {
    return false;
  }

  const bool loaded = LoadArticulation(
      assetManager.GetAcousticGuitarArticulation(), MixerBus::Guitar);
  if (loaded) {
    ResetStringState();
  }
  return loaded;
}

void GuitarVoiceManager::ProcessMIDI(
    const std::vector<MIDI::MIDIEvent> &events) {
  for (const auto &event : events) {
    const int stringId =
        (event.channel >= 1 && event.channel <= 6)
            ? static_cast<int>(event.channel) - 1
            : -1;

    if (event.type == MIDI::MIDIEventType::NoteOn && event.data2 > 0) {
      if (stringId >= 0 && stringId < static_cast<int>(stringActiveNotes_.size())) {
        ++stringActiveNotes_[static_cast<size_t>(stringId)];
        ChokeVoicesOnString(stringId);
      }

      StartVoiceForEvent(event, stringId);
      continue;
    }

    if (event.type != MIDI::MIDIEventType::NoteOff &&
        !(event.type == MIDI::MIDIEventType::NoteOn && event.data2 == 0)) {
      continue;
    }

    if (stringId >= 0 && stringId < static_cast<int>(stringActiveNotes_.size())) {
      --stringActiveNotes_[static_cast<size_t>(stringId)];

      if (stringActiveNotes_[static_cast<size_t>(stringId)] > 0) {
        continue;
      }

      if (stringActiveNotes_[static_cast<size_t>(stringId)] < 0) {
        stringActiveNotes_[static_cast<size_t>(stringId)] = 0;
      }
    }

    StopVoiceForEvent(event, stringId);
  }
}

void GuitarVoiceManager::ResetStringState() {
  stringActiveNotes_.fill(0);
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
