#pragma once

#include "src/core/arrangement/ChordTrack.h"
#include <vector>
#include <cstdint>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// ---------------------------------------------------------
// Style & Contour Enums
// ---------------------------------------------------------
enum class PianoStyle : uint8_t {
    PopBlock,
    SingerSongwriter,
    JazzShell
};

enum class LHStrategy : uint8_t {
    RootFifth,
    RootTenth,
    RootOnly,
    Shell
};

// Controls phrase-level intent for the soprano trajectory.
// Unlike local gravity bias, this plans an actual pitch path first
// and then snaps it to available chord tones.
enum class SopranoContour : uint8_t {
    Hold,   // Flat trajectory — stay near initial pitch. Pedal feel.
    Rise,   // Linear ascent across the phrase.
    Fall,   // Linear descent across the phrase.
    Arch    // Parabolic arc: rise to midpoint apex, then descend.
};

enum class PianoTargetRole : uint8_t {
    LH_Root = 0,
    LH_Fifth = 1,
    LH_Octave = 2,
    LH_ShellLow = 3,

    RH_GuideLow = 4,
    RH_GuideHigh = 5,
    RH_Inner = 6,
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
        for (int r = static_cast<int>(PianoTargetRole::LH_Root); r <= static_cast<int>(PianoTargetRole::LH_ShellLow); ++r) {
            if (pitches[r] > upper) upper = pitches[r];
        }
        if (root == 0 || upper == 0) return 0;
        return upper - root;
    }

    int RHDensity() const {
        int count = 0;
        for (int r = static_cast<int>(PianoTargetRole::RH_GuideLow); r <= static_cast<int>(PianoTargetRole::RH_Top); ++r) {
            if (pitches[r] != 0) ++count;
        }
        return count;
    }
};

// ---------------------------------------------------------
// Piano Voicing Planner
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

    LHStrategy m_lhStrategy;
    int m_rhMinPitch;
    int m_maxRHSpan;
    int m_minVoiceSep;
    bool m_allowRHInner;

    // Pass 1: Pre-compute a target soprano trajectory for the entire phrase
    std::vector<int> PlanSopranoTrajectory(size_t phraseLen) const;

    // Pass 2: Solve outer voices (Bass + Soprano) using planned trajectory
    void SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                          const std::vector<int>& sopranoPath,
                          std::vector<PianoVoicing>& t) const;

    // Pass 3: Inner voice fill + RH_Inner color tone
    void SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                          std::vector<PianoVoicing>& t) const;
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
