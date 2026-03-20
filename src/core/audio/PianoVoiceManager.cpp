#include "PianoVoiceManager.h"

#include "AssetManager.h"

namespace Sonatrix {
namespace Core {
namespace Audio {

bool PianoVoiceManager::LoadAcousticPianoKit(const std::string &assetsAbsolutePath) {
    auto &assetManager = AssetManager::GetInstance();
    if (!assetManager.LoadAcousticPianoAnchors(assetsAbsolutePath)) {
        return false;
    }

    return LoadArticulation(assetManager.GetAcousticPianoArticulation(),
                            MixerBus::Piano);
}

void PianoVoiceManager::ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) {
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
