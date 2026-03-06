#include "StringCompiler.h"

namespace Sonatrix {
namespace Core {
namespace Engines {

std::unique_ptr<MIDI::IMIRCompiler> CreateStringEngine() {
    return std::make_unique<StringCompiler>();
}

MIDI::MIDIStream StringCompiler::CompileClip(
    const EditorClip& clip, 
    const std::vector<ChordTrackEvent>& /*chordTimeline*/
) const {
    MIDI::MIDIStream stream;
    ActiveChordContext mockContext;
    
    for (const auto& mir : clip.basePattern->events) {
        if (mir.type != ArticulationType::GenericNote && mir.type != ArticulationType::StringSwell) {
            continue;
        }
        
        MusicalTime triggerTime = clip.timelineStart + mir.offsetMap;
        MusicalTime offTime = triggerTime + BeatsToTime(mir.lengthBeats);
        
        // 1. Divisi Range Clamping
        // E.g. pair<MIDI Channel, Pitch Number>
        auto divisiAllocation = AllocateDivisi(mockContext);
        
        for (const auto& [channel, pitch] : divisiAllocation) {
            stream.events.push_back({triggerTime, MIDI::MIDIEventType::NoteOn, channel, pitch, mir.velocityBase});
            stream.events.push_back({offTime, MIDI::MIDIEventType::NoteOff, channel, pitch, 0});
        }
        
        // 2. Sweeping Dynamics Pass
        if (mir.type == ArticulationType::StringSwell) {
            CalculatePredictiveExpressionCurve(stream, triggerTime, offTime);
        }
    }
    
    stream.SortByTime();
    return stream;
}

std::vector<std::pair<uint8_t, uint8_t>> StringCompiler::AllocateDivisi(const ActiveChordContext& /*targetChord*/) const {
    // In production, analyzes pitch structure:
    // If 4-note chord:
    // Channel 1 (Vln 1): Note 4 (Top)
    // Channel 2 (Vln 2): Note 3 
    // Channel 3 (Vla): Note 2
    // Channel 4 (Vc): Note 1 (Bottom)
    
    std::vector<std::pair<uint8_t, uint8_t>> mockDivisi;
    mockDivisi.push_back({1, 72}); // Vln Top
    mockDivisi.push_back({2, 67}); // Vln Mid
    mockDivisi.push_back({3, 64}); // Vla
    mockDivisi.push_back({4, 48}); // Vc anchor
    
    return mockDivisi;
}

void StringCompiler::CalculatePredictiveExpressionCurve(
    MIDI::MIDIStream& outStream,
    MusicalTime chordStartTime,
    MusicalTime chordEndTime
) const {
    // Produces a smooth curve of CC11 (Expression) events spanning the time window.
    // The curve scales exponentially the closer we get to chordEndTime.
    
    int64_t totalTicks = (chordEndTime - chordStartTime).ticks;
    int steps = 16;
    int64_t tickStep = totalTicks / steps;
    
    for (int i = 0; i < steps; ++i) {
        MusicalTime ccTime = chordStartTime + MusicalTime(tickStep * i);
        
        // Simple linear fade for mock:
        uint8_t ccVal = static_cast<uint8_t>((static_cast<float>(i) / static_cast<float>(steps)) * 127.0f);
        
        outStream.events.push_back({ccTime, MIDI::MIDIEventType::ControlChange, 1, 11, ccVal}); // Ch 1 Vlns
        outStream.events.push_back({ccTime, MIDI::MIDIEventType::ControlChange, 2, 11, ccVal});
        outStream.events.push_back({ccTime, MIDI::MIDIEventType::ControlChange, 3, 11, ccVal});
        outStream.events.push_back({ccTime, MIDI::MIDIEventType::ControlChange, 4, 11, ccVal});
    }
}

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
