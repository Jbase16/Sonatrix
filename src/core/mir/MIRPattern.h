#pragma once

#include "MusicalTime.h"
#include <cstdint>
#include <vector>

namespace Sonatrix {
namespace Core {

// -----------------------------------------------------------------------------
// The Musical Intermediate Representation (MIR)
// Patterns are NOT stored as raw audio or raw MIDI. 
// They are stored as structural intent that the engines compile just-in-time.
// -----------------------------------------------------------------------------

enum class ArticulationType : uint8_t {
    // Shared
    GenericNote, // Piano, Bass
    
    // Guitar Specific
    GuitarDownstroke,
    GuitarUpstroke,
    GuitarMute,
    GuitarPalmDrop,
    
    // String Specific
    StringSwell,
    StringSpiccato,
    
    // Drum Specific
    DrumHit
};

// A single stroke or intent command within a pattern
struct MIREvent {
    MusicalTime offsetMap;      // Position relative to start of pattern (in ticks)
    double lengthBeats;         // Duration of the intent
    uint8_t velocityBase{100};  // 0-127 intent velocity
    ArticulationType type{ArticulationType::GenericNote};
    
    // Meaning depends on the instrument:
    // Guitar: 0 = full chord, 1 = low strings only, 2 = high strings only.
    // Piano:  Note interval relative to chord root.
    // Drums:  Kit piece ID (e.g., 1 = Kick, 2 = Snare)
    int16_t actionParameter{0}; 
};

// A read-only memory representation of a musical pattern (e.g., "Pop Rock Verse")
struct MIRPattern {
    MusicalTime totalLength;
    std::vector<MIREvent> events;
    
    // Metadata for the engine to know how to interpret actionParameters
    enum class TargetEngine { Guitar, Piano, Bass, Strings, Drums };
    TargetEngine intendedEngine{TargetEngine::Guitar};
};

} // namespace Core
} // namespace Sonatrix
