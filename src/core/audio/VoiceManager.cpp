#include "VoiceManager.h"
#include <limits>

namespace Sonatrix {
namespace Core {
namespace Audio {

void VoiceManager::ProcessMIDI(const std::vector<MIDI::MIDIEvent>& events, InstrumentArticulation& articulation) {
    for (const auto& ev : events) {
        if (ev.type == MIDI::MIDIEventType::NoteOn && ev.data2 > 0) {
            
            // 1. Determine which physical acoustic zone to load based on the sparse matrix
            const SampleZone* zone = articulation.FindZone(ev.data1, ev.data2);
            if (!zone) continue; // No matching sample found
            
            // 2. Allocate a voice (stealing if necessary)
            AudioVoice* v = GetBestAvailableVoice();
            if (v) {
                // If it was stolen while sustaining, forcefully stop it first (to avoid clicks ideally 
                // we'd do a 5ms fast-fade, but we mock the immediate override here).
                v->Start(zone, ev.data1, ev.data2);
            }
            
        } else if (ev.type == MIDI::MIDIEventType::NoteOff || (ev.type == MIDI::MIDIEventType::NoteOn && ev.data2 == 0)) {
            // Find the active voice playing this pitch and release it.
            // Note: In an actual implementation, we map MIDI Channel and active Note IDs 
            // to ensure we don't kill a newer note with the same pitch.
            // Simplified for demonstration.
            // (Mock constraint solver handles this logic upstream).
        }
    }
}

AudioVoice* VoiceManager::GetBestAvailableVoice() {
    AudioVoice* worstVoice = nullptr;
    float lowestPriority = std::numeric_limits<float>::max();
    
    for (auto& v : voices_) {
        if (v.IsFree()) {
            return &v; // Immediate success
        }
        
        float p = v.GetStealingPriority();
        if (p < lowestPriority) {
            lowestPriority = p;
            worstVoice = &v;
        }
    }
    
    // Voice Stealing occurred. We return the active voice with the lowest volume/envelope priority.
    return worstVoice; 
}

void VoiceManager::RenderAudio(float* outputBuffer, uint32_t numFrames, uint32_t numChannels) {
    // Process every voice and accumulate into the final output buffer
    for (auto& v : voices_) {
        v.RenderNextBlock(outputBuffer, numFrames, numChannels);
    }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
