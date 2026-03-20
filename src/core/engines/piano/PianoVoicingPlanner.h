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
// Soprano Contour Mode
// ---------------------------------------------------------
// Controls phrase-level intent for the top-voice trajectory.
// The Macro pass biases tone selection based on position within the phrase.
enum class SopranoContour : uint8_t {
    Hold,   // Minimize movement. Favor common tones. Static pedal feel.
    Rise,   // Bias upward motion across the phrase. Tension building.
    Fall,   // Bias downward motion. Resolution / closing feel.
    Arch    // Rise through first half, fall through second half.
};

// ---------------------------------------------------------
// Semantic Roles for Piano Note Mapping
// ---------------------------------------------------------
enum class PianoTargetRole : uint8_t {
    LH_Root = 0,
    LH_Fifth = 1,
    LH_Octave = 2,
    LH_ShellLow = 3,

    RH_GuideLow = 4,
    RH_GuideHigh = 5,
    RH_Inner = 6,      // Color tone (9th, 5th, or tension depending on style)
    RH_Top = 7
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

    int RHSpan() const {
        int top = pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        // Find lowest populated RH voice
        int low = 127;
        for (int r = static_cast<int>(PianoTargetRole::RH_GuideLow); r <= static_cast<int>(PianoTargetRole::RH_Top); ++r) {
            if (pitches[r] != 0 && pitches[r] < low) low = pitches[r];
        }
        if (top == 0 || low == 127) return 0;
        return top - low;
    }

    int LHSpan() const {
        int root = pitches[static_cast<int>(PianoTargetRole::LH_Root)];
        int upper = 0;
        // Find highest populated LH voice
        for (int r = static_cast<int>(PianoTargetRole::LH_Root); r <= static_cast<int>(PianoTargetRole::LH_ShellLow); ++r) {
            if (pitches[r] > upper) upper = pitches[r];
        }
        if (root == 0 || upper == 0) return 0;
        return upper - root;
    }

    // Count of populated RH voices (density metric)
    int RHDensity() const {
        int count = 0;
        for (int r = static_cast<int>(PianoTargetRole::RH_GuideLow); r <= static_cast<int>(PianoTargetRole::RH_Top); ++r) {
            if (pitches[r] != 0) ++count;
        }
        return count;
    }
};

// ---------------------------------------------------------
// Piano Voicing Planner (Hierarchical Contrapuntal Solver)
// ---------------------------------------------------------
class PianoVoicingPlanner {
public:
    explicit PianoVoicingPlanner(PianoStyle style = PianoStyle::PopBlock,
                                 SopranoContour contour = SopranoContour::Hold);
    ~PianoVoicingPlanner() = default;

    std::vector<PianoVoicing> SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) const;

private:
    PianoStyle m_style;
    SopranoContour m_contour;

    // Derived from style
    LHStrategy m_lhStrategy;
    int m_rhMinPitch;
    int m_maxRHSpan;
    int m_minVoiceSep;
    bool m_allowRHInner;   // Whether to populate the color-tone seat

    // Phase A: Outer Shell (Bass + Soprano)
    void SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const;

    // Phase B: Inner Voice Fill + RH_Inner color tone
    void SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const;
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
