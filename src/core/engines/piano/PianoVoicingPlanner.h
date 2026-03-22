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

enum class SopranoContour : uint8_t {
    Hold,
    Rise,
    Fall,
    Arch
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
// Harmonic Function Classification
// ---------------------------------------------------------
enum class HarmonicFunction : uint8_t {
    Default,            // no special classification
    Pedal,              // same bass, different harmony
    DominantResolution, // root descends P5, previous has tritone
    ReinterpretiveHold  // 3+ shared PCs, root changes
};

// ---------------------------------------------------------
// Chord Obligation (per quality × style)
// ---------------------------------------------------------
struct ChordObligation {
    std::vector<int> required;
    std::vector<int> preferred;
    int maxDesiredDensity;
};

// ---------------------------------------------------------
// Transition Context
// ---------------------------------------------------------
struct TransitionContext {
    HarmonicFunction function = HarmonicFunction::Default;
    int sharedPitchClasses = 0;
    float continuityWeight = 1.0f;
    int sufficiencyGate = 100;
};

// ---------------------------------------------------------
// Obligation Coverage
// ---------------------------------------------------------
struct ObligationCoverage {
    ChordObligation obligation;
    std::vector<int> coveredPcs;
    std::vector<int> unmetRequired;
    std::vector<int> unmetPreferred;
};

// ---------------------------------------------------------
// Voicing Explanation (structured debug output)
// ---------------------------------------------------------
struct VoicingExplanation {
    int totalRequired = 0;
    int coveredBySoprano = 0;
    int coveredByBass = 0;
    int coveredByLh2 = 0;
    int suppliedByTuple = 0;
    int unmet = 0;
    bool sufficient = false;
    HarmonicFunction transitionFunction = HarmonicFunction::Default;
    int sufficiencyGateUsed = 100;
    float continuityWeightUsed = 1.0f;
    int candidatesEvaluated = 0;
    int candidatesRejected = 0;
};

// ---------------------------------------------------------
// Tiered Score
// ---------------------------------------------------------
struct TieredScore {
    int harmonic;
    float continuity;
    int density;

    bool operator>(const TieredScore& o) const {
        if (harmonic != o.harmonic) return harmonic > o.harmonic;
        if (continuity != o.continuity) return continuity > o.continuity;
        return density > o.density;
    }
};

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

    std::vector<PianoVoicing> SolveTimeline(
        const std::vector<ChordTrackEvent>& chordTimeline,
        std::vector<VoicingExplanation>* explanations = nullptr) const;

private:
    PianoStyle m_style;
    SopranoContour m_contour;

    LHStrategy m_lhStrategy;
    int m_rhMinPitch;
    int m_maxRHSpan;
    int m_minVoiceSep;
    bool m_allowRHInner;

    std::vector<int> PlanSopranoTrajectory(size_t phraseLen) const;

    void SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                          const std::vector<int>& sopranoPath,
                          std::vector<PianoVoicing>& t) const;

    void SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                          std::vector<PianoVoicing>& t,
                          std::vector<VoicingExplanation>* explanations) const;
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
