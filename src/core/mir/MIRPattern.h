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
    // Shared / Keyboard
    GenericNote, // Piano, Bass
    PianoChord,
    PianoArpeggioUp,
    PianoArpeggioDown,
    
    // Guitar Specific
    GuitarDownstroke,
    GuitarUpstroke,
    GuitarPluck,
    GuitarPinch,
    GuitarMute,
    GuitarPalmDrop,
    
    // Bass Specific
    BassSlap,
    BassPop,

    // String Specific
    StringSwell,
    StringSpiccato,
    
    // Drum Specific
    DrumHit,
    DrumGhostNote
};

enum class GuitarTargetRole : uint8_t {
    None,
    Bass,
    AltBass,
    InnerLow,
    InnerHigh,
    Treble,
    Top
};

enum class GuitarVoicingMode : uint8_t {
    Default,
    AcousticOpen
};

// A single stroke or intent command within a pattern
struct MIREvent {
    MusicalTime offsetMap;      // Position relative to start of pattern (in ticks)
    double lengthBeats;         // Duration of the intent
    uint8_t velocityBase{100};  // 0-127 intent velocity
    ArticulationType type{ArticulationType::GenericNote};
    
    // Meaning depends on the instrument:
    // Guitar: legacy raw string index / pinch bitmask routing.
    // Piano:  Note interval relative to chord root.
    // Drums:  Kit piece ID (e.g., 1 = Kick, 2 = Snare)
    int16_t actionParameter{0}; 

    // Guitar-specific semantic targets for reusable picking patterns.
    // Legacy raw string indices and pinch bitmasks remain supported through
    // actionParameter when these are left as None.
    GuitarTargetRole guitarTargetRole{GuitarTargetRole::None};
    GuitarTargetRole guitarSecondaryTargetRole{GuitarTargetRole::None};

    bool UsesGuitarTargetRoles() const {
        return guitarTargetRole != GuitarTargetRole::None ||
               guitarSecondaryTargetRole != GuitarTargetRole::None;
    }
};

// A read-only memory representation of a musical pattern (e.g., "Pop Rock Verse")
struct MIRPattern {
    MusicalTime totalLength;
    std::vector<MIREvent> events;
    GuitarVoicingMode guitarVoicingMode{GuitarVoicingMode::Default};
    
    // Metadata for the engine to know how to interpret actionParameters
    enum class TargetEngine { Guitar, Piano, Bass, Strings, Drums };
    TargetEngine intendedEngine{TargetEngine::Guitar};
};

} // namespace Core
} // namespace Sonatrix
