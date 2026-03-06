#include "BassCompiler.h"

namespace Sonatrix {
namespace Core {
namespace Engines {

std::unique_ptr<MIDI::IMIRCompiler> CreateBassEngine(const ML::DynamicGrooveVector& vector) {
    return std::make_unique<BassCompiler>(vector);
}

BassCompiler::BassCompiler(const ML::DynamicGrooveVector& globalGrooveVector)
    : grooveVector_(globalGrooveVector) {}

MIDI::MIDIStream BassCompiler::CompileClip(
    const EditorClip& clip, 
    const std::vector<ChordTrackEvent>& chordTimeline
) const {
    MIDI::MIDIStream stream;
    
    for (const auto& mir : clip.basePattern->events) {
        if (mir.type != ArticulationType::GenericNote) continue;
        
        MusicalTime eventAbsoluteGridTime = clip.timelineStart + mir.offsetMap;
        
        // 1. Determine Harmonic Context (What note is the Bass playing?)
        uint8_t pitch = GetBassPitchForTime(eventAbsoluteGridTime, chordTimeline);
        
        // 2. PHASE LOCKING: Query the Drum Engine's groove vector!
        int64_t phaseOffset = 0;
        double velocityScale = 1.0;
        
        auto grooveOpt = grooveVector_.GetOffsetForSubbeat(eventAbsoluteGridTime);
        if (grooveOpt) {
            // The bass note actively warps to lock to the drum's human deviation
            phaseOffset = grooveOpt->deviationTicks;
            velocityScale = grooveOpt->velocityMultiplier;
        }
        
        // 3. Generate Semantic MIDI
        MusicalTime actualPerformanceTime = eventAbsoluteGridTime + MusicalTime(phaseOffset);
        uint8_t actualVelocity = static_cast<uint8_t>(mir.velocityBase * velocityScale);
        
        // Ensure velocity constraints
        if (actualVelocity > 127) actualVelocity = 127;
        
        stream.events.push_back({actualPerformanceTime, MIDI::MIDIEventType::NoteOn, 1, pitch, actualVelocity});
        
        MusicalTime offTime = actualPerformanceTime + BeatsToTime(mir.lengthBeats);
        stream.events.push_back({offTime, MIDI::MIDIEventType::NoteOff, 1, pitch, 0});
    }
    
    stream.SortByTime();
    return stream;
}

uint8_t BassCompiler::GetBassPitchForTime(MusicalTime /*time*/, const std::vector<ChordTrackEvent>& chordTimeline) const {
    // In a real implementation we binary search the chord timeline.
    if (!chordTimeline.empty()) {
        // e.g., PitchClass::C -> MIDI note 36 (C2)
        return 36 + static_cast<uint8_t>(chordTimeline.front().chord.overBass);
    }
    return 36; // Default to C2
}

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
