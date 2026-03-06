#pragma once

#include "MusicalTime.h"
#include "MIRPattern.h"
#include <vector>
#include <memory>
#include <optional>

namespace Sonatrix {
namespace Core {

// -----------------------------------------------------------------------------
// Delta Graph System
// Manages non-destructive user edits on top of generated MIR data.
// This allows the base sequence to change chords dynamically, while
// still obeying the rhythmic overrides (deletions, articulations limits)
// set by the user (the "stomp out" vs "chord change" problem).
// -----------------------------------------------------------------------------

enum class DeltaOperation : uint8_t {
    // Rhythmic / Articulation overrides
    MuteStroke,          // Prevent the engine from playing this sub-beat
    ForceArticulation,   // e.g. "Override whatever it thought to a heavy palm mute"
    ModifyVelocity,
    
    // Harmonic overrides (mostly for piano/bass passing notes)
    OffsetPitch          // e.g. "Shift this engine-generated root down a 5th"
};

// Represents a single user modification layered on top of the engine
struct DeltaNode {
    MusicalTime startOffset;
    double windowBeats{0.1}; // The span of time this rule applies to
    
    DeltaOperation op{DeltaOperation::MuteStroke};
    
    // The value associated with the operation (velocity scale, pitch offset, articulation ID)
    int16_t parameterValue{0};
};

// A clip placed on the arrangement timeline
class EditorClip {
public:
    EditorClip(std::shared_ptr<const MIRPattern> sourcePattern) 
        : basePattern(sourcePattern) {}
        
    // The global time this clip starts
    MusicalTime timelineStart;
    
    // The base performance intent (read-only)
    std::shared_ptr<const MIRPattern> basePattern;
    
    // User edits layered on top, applied Just-In-Time during rendering
    std::vector<DeltaNode> overrides;
    
    // Applies the delta sequence to a compiled event stream.
    // To be called by the discrete instrument engines (Guitar, Piano) before voice allocation.
    void ApplyDeltas(std::vector<MIREvent>& streamToCompile) const;
};

} // namespace Core
} // namespace Sonatrix
