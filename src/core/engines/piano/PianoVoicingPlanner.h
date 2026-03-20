#pragma once

#include "src/core/arrangement/ChordTrack.h"
#include <vector>
#include <cstdint>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// ---------------------------------------------------------
// Semantic Roles for Piano Note Mapping
// ---------------------------------------------------------
// Unlike the Bass engine which maps intervals directly, the Piano
// engine maps MIR actions to these semantic topological roles.
enum class PianoTargetRole : uint8_t {
    LH_Root = 0,
    LH_Fifth = 1,
    LH_Octave = 2,
    LH_ShellLow = 3,  // Usually the 10th or 7th above the root
    
    RH_GuideLow = 4,  // Lowest essential chord tone in right hand (usually 3rd or 7th)
    RH_GuideHigh = 5, // Next essential chord tone
    RH_Inner = 6,     // Optional color note (9th, 5th, etc.)
    RH_Top = 7        // Highest melody note of the voicing
};

// ---------------------------------------------------------
// Piano Voicing Node
// ---------------------------------------------------------
// Represents a single, physical layout of notes on the keyboard
// for a specific chord in the timeline.
struct PianoVoicing {
    uint8_t pitches[8] = {0}; // Indexed by PianoTargetRole
    
    // Evaluative metrics for transition/energy scoring
    int lhCenter = 0;
    int rhCenter = 0;
    int topPitch = 0;
    
    bool IsValid() const {
        return pitches[static_cast<int>(PianoTargetRole::LH_Root)] != 0;
    }
    
    uint8_t GetPitch(PianoTargetRole role) const {
        return pitches[static_cast<int>(role)];
    }
};

// ---------------------------------------------------------
// Piano Voicing Planner (Hierarchical Contrapuntal Solver)
// ---------------------------------------------------------
// Generates a structural voice-leading graph in distinct passes:
// 1. Macro: Outer shells (Soprano and Bass contour).
// 2. Meso: Neo-Riemannian inner voice fluid filling.
class PianoVoicingPlanner {
public:
    PianoVoicingPlanner();
    ~PianoVoicingPlanner() = default;

    // The main engine entry point:
    bool SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline);

    // Returns the cached optimum.
    PianoVoicing GetVoicingForChordIndex(size_t index) const;

private:
    std::vector<PianoVoicing> m_solvedTimeline;

    // Phase A: Macro Constraints
    // Analyzes the chord progression to determine logical top-line (Soprano)
    // and bottom-line (Bass) trajectories based on phrase arc.
    void SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline);
    
    // Phase B: Meso Fluidity
    // Given locked outer limits, pours inner guide tones (3/7/tensions)
    // into the remaining space minimizing vertical movement.
    void SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline);
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
