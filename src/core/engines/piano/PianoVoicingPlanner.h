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
// Piano Voicing Planner
// ---------------------------------------------------------
// An energy-based constraint solver. It does not output MIDI.
// It generates candidates for each chord and uses Viterbi pathfinding
// to find the sequence with the lowest transition energy (smooth voice-leading).
class PianoVoicingPlanner {
public:
    PianoVoicingPlanner();
    ~PianoVoicingPlanner() = default;

    // The main engine entry point:
    // Takes the full song timeline and pre-calculates the optimal 
    // sequence of voicings, cached internally.
    bool SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline);

    // Returns the cached optimum. If un-solved, generates a naive default.
    PianoVoicing GetVoicingForChordIndex(size_t index) const;

private:
    std::vector<std::vector<PianoVoicing>> m_candidatesPerChord;
    std::vector<PianoVoicing> m_solvedTimeline;

    // Step 1: Combinatorial Space
    void GenerateCandidates(const ChordTrackEvent& event, std::vector<PianoVoicing>& outVoicings) const;
    
    // Step 2: Energy/Cost Evaluation
    float CalculateTransitionCost(const PianoVoicing& from, const PianoVoicing& to) const;
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
