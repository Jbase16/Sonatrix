#include "GuitarCompiler.h"
#include <iostream> 

namespace Sonatrix {
namespace Core {
namespace MIDI {

std::unique_ptr<IMIRCompiler> CreateGuitarEngine() {
    return std::make_unique<GuitarCompiler>();
}

MIDIStream GuitarCompiler::CompileClip(
    const EditorClip& clip, 
    const std::vector<ChordTrackEvent>& // chordTimeline
) const {
    MIDIStream stream;
    
    // In a real implementation:
    // 1. We look at clip.timelineStart
    // 2. We find the matching ChordTrackEvent for that time window
    // 3. We create an internal Voice configuration (6 strings) using EvaluateVoiceLeadingCost
    // 4. We iterate through clip.basePattern->events
    // 5. We apply clip.overrides (the DeltaGraph) to the stream *before* emitting
    
    for (const auto& mir : clip.basePattern->events) {
        MusicalTime eventAbsoluteTime = clip.timelineStart + mir.offsetMap;
        
        // This simulates applying a Delta Override (e.g. MuteStroke)
        bool isMuted = false;
        for (const auto& delta : clip.overrides) {
            // Delta Graph Collision Detection
            if (delta.startOffset == mir.offsetMap && delta.op == DeltaOperation::MuteStroke) {
                isMuted = true;
                break;
            }
        }
        
        if (!isMuted) {
            EmitStrum(stream, eventAbsoluteTime, mir.type, mir.velocityBase);
        }
    }
    
    stream.SortByTime();
    return stream;
}

int GuitarCompiler::EvaluateVoiceLeadingCost(const ActiveChordContext& /*target*/) const {
    // PhD Component: A* Search or Constraint solving
    // Currently mocked: returns lowest theoretical cost.
    return 0; // Ideal voice leading assumed
}

void GuitarCompiler::EmitStrum(
    MIDIStream& outStream, 
    MusicalTime baseTime, 
    ArticulationType direction, 
    uint8_t baseVelocity
) const {
    
    // Assume we solved for open G Major [E3, B3, G4, D4, B4, G5]
    // 1PPQN = roughly 1ms at 120bpm for 960 res. 
    // We disperse the strums sequentially by 10-15 ticks to mimic a pick crossing 6 strings.
    
    int64_t dispersionTicks = (direction == ArticulationType::GuitarDownstroke) ? 15 : -15;
    uint8_t strings[6] = {43, 47, 50, 55, 59, 67}; // Literal MIDI notes
    
    if (direction == ArticulationType::GuitarUpstroke) {
        // Reverse array for upstroke
        for(int i = 0; i < 3; ++i) std::swap(strings[i], strings[5-i]);
    }
    
    int64_t accumulatedOffset = 0;
    for (int i = 0; i < 6; ++i) {
        MusicalTime triggerTime = baseTime + MusicalTime(accumulatedOffset);
        
        // Note On
        outStream.events.push_back( {triggerTime, MIDIEventType::NoteOn, 0, strings[i], baseVelocity} );
        
        // Note Off (Generic 1.0 beat duration for demo)
        MusicalTime endTime = triggerTime + BeatsToTime(1.0);
        outStream.events.push_back( {endTime, MIDIEventType::NoteOff, 0, strings[i], 0} );
        
        accumulatedOffset += dispersionTicks;
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
