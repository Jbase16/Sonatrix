#pragma once

#include "src/core/arrangement/ChordTrack.h"
#include <vector>
#include <cstdint>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// ---------------------------------------------------------
// Style Presets
// ---------------------------------------------------------
// Controls left-hand strategy, right-hand density, and 
// chord-tone priority ranking.
enum class PianoStyle : uint8_t {
    PopBlock,           // Root+Fifth LH, tight RH triads, 5th optional
    SingerSongwriter,   // Root+Tenth LH, open RH spacing, add9 friendly
    JazzShell           // Root+7th LH (rootless option), RH guide tones + tensions
};

// ---------------------------------------------------------
// Left-Hand Strategy
// ---------------------------------------------------------
enum class LHStrategy : uint8_t {
    RootFifth,    // Root + Perfect 5th
    RootTenth,    // Root + 3rd one octave up (10th interval)
    RootOnly,     // Root alone (sparse ballad)
    Shell         // Root + 3rd or Root + 7th (jazz shell voicing)
};

// ---------------------------------------------------------
// Semantic Roles for Piano Note Mapping
// ---------------------------------------------------------
enum class PianoTargetRole : uint8_t {
    LH_Root = 0,
    LH_Fifth = 1,
    LH_Octave = 2,
    LH_ShellLow = 3,  // 10th, 7th, or 3rd above root depending on strategy
    
    RH_GuideLow = 4,  // Lowest essential chord tone in right hand (usually 3rd or 7th)
    RH_GuideHigh = 5,  // Next essential chord tone
    RH_Inner = 6,      // Optional color note (9th, 5th, etc.)
    RH_Top = 7         // Highest melody note of the voicing
};

static constexpr int kPianoRoleCount = 8;

// ---------------------------------------------------------
// Piano Voicing Node
// ---------------------------------------------------------
struct PianoVoicing {
    uint8_t pitches[kPianoRoleCount] = {0};
    
    bool IsValid() const {
        return pitches[static_cast<int>(PianoTargetRole::LH_Root)] != 0;
    }
    
    uint8_t GetPitch(PianoTargetRole role) const {
        return pitches[static_cast<int>(role)];
    }

    // Hand geometry diagnostics
    int RHSpan() const {
        int top = pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        int low = pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)];
        if (top == 0 || low == 0) return 0;
        return top - low;
    }

    int LHSpan() const {
        int root = pitches[static_cast<int>(PianoTargetRole::LH_Root)];
        int upper = pitches[static_cast<int>(PianoTargetRole::LH_Fifth)];
        if (pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] != 0) {
            upper = pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)];
        }
        if (root == 0 || upper == 0) return 0;
        return upper - root;
    }
};

// ---------------------------------------------------------
// Piano Voicing Planner (Hierarchical Contrapuntal Solver)
// ---------------------------------------------------------
// Generates a structural voice-leading graph in distinct passes:
// 1. Macro: Outer shells (Soprano and Bass contour).
// 2. Meso: Inner voice fluid filling with spacing constraints.
class PianoVoicingPlanner {
public:
    explicit PianoVoicingPlanner(PianoStyle style = PianoStyle::PopBlock);
    ~PianoVoicingPlanner() = default;

    // Takes the full timeline and returns the solved structural voicings.
    std::vector<PianoVoicing> SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) const;

private:
    PianoStyle m_style;

    // Derived from style at construction
    LHStrategy m_lhStrategy;
    int m_rhMinPitch;      // Lowest allowed RH pitch (muddy register gate)
    int m_maxRHSpan;       // Max semitones between RH_Top and RH_GuideLow
    int m_minVoiceSep;     // Min semitones between adjacent RH voices

    // Phase A: Macro Constraints (Outer Shell)
    void SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& inOutTimeline) const;
    
    // Phase B: Meso Fluidity (Inner Voice Fill with Constraints)
    void SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& inOutTimeline) const;
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
