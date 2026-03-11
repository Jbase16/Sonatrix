#pragma once

#include "../mir/MusicalTime.h"
#include <cstdint>
#include <string_view>

namespace Sonatrix {
namespace Core {

// Represents the pitch class (0 = C, 1 = C#, ..., 11 = B)
enum class PitchClass : uint8_t {
    C = 0, C_Sharp = 1, D = 2, D_Sharp = 3, E = 4, F = 5,
    F_Sharp = 6, G = 7, G_Sharp = 8, A = 9, A_Sharp = 10, B = 11
};

// Represents common chord qualities
enum class ChordQuality : uint8_t {
    Major, Minor, Diminished, Augmented,
    Dominant7, Major7, Minor7, HalfDiminished7,
    Sus2, Sus4, Add9, PowerChord
};

// Represents a harmonic context at a specific point in time
struct ActiveChordContext {
    PitchClass root{PitchClass::C};
    ChordQuality quality{ChordQuality::Major};
    
    // Optional specified bass note for inversions (e.g., C/E)
    // If same as root, it's root position.
    PitchClass overBass{PitchClass::C}; 
    
    // Additional tensions (9, 11, 13) handled by a bitmask or extension array in the future
    uint16_t extensionsMask{0};
    
    constexpr bool isRootPosition() const {
        return root == overBass;
    }
};

// An event on the global Chord Track
struct ChordTrackEvent {
    MusicalTime position;
    ActiveChordContext chord;
};

} // namespace Core
} // namespace Sonatrix
