#include "DrumCompiler.h"

namespace Sonatrix {
namespace Core {
namespace Engines {

std::unique_ptr<MIDI::IMIRCompiler> CreateDrumEngine(ML::DynamicGrooveVector& vector) {
    return std::make_unique<DrumCompiler>(vector);
}

DrumCompiler::DrumCompiler(ML::DynamicGrooveVector& globalGrooveVector)
    : grooveVector_(globalGrooveVector) {}

MIDI::MIDIStream DrumCompiler::CompileClip(
    const EditorClip& clip, 
    const std::vector<ChordTrackEvent>& // Chords don't affect drums
) const {
    MIDI::MIDIStream stream;
    
    for (const auto& mir : clip.basePattern->events) {
        if (mir.type != ArticulationType::DrumHit) continue;
        
        MusicalTime eventAbsoluteGridTime = clip.timelineStart + mir.offsetMap;
        
        // 1. Check DeltaGraph (User overrides like Mute)
        bool isMuted = false;
        for (const auto& delta : clip.overrides) {
            if (delta.startOffset == mir.offsetMap && delta.op == DeltaOperation::MuteStroke) {
                isMuted = true;
                break;
            }
        }
        
        if (isMuted) continue;
        
        // 2. Latent Groove Extraction
        // Extract the human feel (the deviation from the rigid absolute grid)
        int64_t deviationTicks = ExtractHumanDeviation(eventAbsoluteGridTime, mir.velocityBase);
        
        // 3. Populate the Master Groove Vector for other instruments
        ML::GrooveOffset offset{
            .anchorTime = eventAbsoluteGridTime,
            .deviationTicks = deviationTicks,
            .velocityMultiplier = 1.0 // Simple mock
        };
        grooveVector_.EmplaceOffset(offset);
        
        // 4. Generate the actual Semantic MIDI (Output to audio engine)
        // apply the deviation to the literal MIDI output stream
        MusicalTime actualPerformanceTime = eventAbsoluteGridTime + MusicalTime(deviationTicks);
        
        uint8_t kitPieceNote = static_cast<uint8_t>(mir.actionParameter); 
        
        stream.events.push_back({actualPerformanceTime, MIDI::MIDIEventType::NoteOn, 9, kitPieceNote, mir.velocityBase});
        
        // NoteOff quickly for drums
        MusicalTime offTime = actualPerformanceTime + MusicalTime(40);
        stream.events.push_back({offTime, MIDI::MIDIEventType::NoteOff, 9, kitPieceNote, 0});
    }
    
    stream.SortByTime();
    return stream;
}

int64_t DrumCompiler::ExtractHumanDeviation(MusicalTime /*absoluteTime*/, uint8_t /*baseVelocity*/) const {
    // PhD Component: Neural inference of Latent Groove
    // Mock: Returns a constant "drag" of 5 ticks for demonstration.
    return 5; 
}

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
