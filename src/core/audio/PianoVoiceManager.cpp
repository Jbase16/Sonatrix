#include "PianoVoiceManager.h"
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace Audio {

bool PianoVoiceManager::LoadAcousticPianoKit(const std::string &assetsAbsolutePath) {
    std::cout << "[PianoVoiceManager] Loading Acoustic Piano kit from: " << assetsAbsolutePath << "\n";
    
    // We define three acoustic piano anchors for v1 polyphony:
    // C3 (48), C4 (60), C5 (72)
    // The pitch offsets scale seamlessly according to the voice engine.

    AddZone(SampleZone{48, assetsAbsolutePath + "/piano_c3.wav", 36, 54}); // E2 to F#3
    AddZone(SampleZone{60, assetsAbsolutePath + "/piano_c4.wav", 55, 66}); // G3 to F#4
    AddZone(SampleZone{72, assetsAbsolutePath + "/piano_c5.wav", 67, 85}); // G4 to C#6
    
    return true;
}

void PianoVoiceManager::ProcessMIDI(const std::vector<MIDI::MIDIEvent> &events) {
    // Phase 19 v2: Add Sustain Pedal Logic here natively over CC #64
    // allowing notes to ring out indefinitely until pedal life.
    // For now, fall back to strict NoteOffs.
    VoiceManager::ProcessMIDI(events);
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
