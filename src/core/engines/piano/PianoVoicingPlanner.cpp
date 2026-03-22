#include "PianoVoicingPlanner.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// ---------------------------------------------------------
// Harmonic Core Helpers
// ---------------------------------------------------------

static std::vector<int> GetPitchClasses(const ActiveChordContext& chord) {
    std::vector<int> pcs;
    int root = static_cast<int>(chord.root);
    pcs.push_back(root);

    int third = -1;
    int fifth = (root + 7) % 12;
    int seventh = -1;

    switch (chord.quality) {
        case ChordQuality::Major:       third = (root + 4) % 12; break;
        case ChordQuality::Minor:       third = (root + 3) % 12; break;
        case ChordQuality::Diminished:  third = (root + 3) % 12; fifth = (root + 6) % 12; break;
        case ChordQuality::Augmented:   third = (root + 4) % 12; fifth = (root + 8) % 12; break;
        case ChordQuality::Dominant7:   third = (root + 4) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::Major7:      third = (root + 4) % 12; seventh = (root + 11) % 12; break;
        case ChordQuality::Minor7:      third = (root + 3) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::HalfDiminished7: third = (root + 3) % 12; fifth = (root + 6) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::Sus2:        third = (root + 2) % 12; break;
        case ChordQuality::Sus4:        third = (root + 5) % 12; break;
        case ChordQuality::Add9:        third = (root + 4) % 12; break;
        case ChordQuality::PowerChord:  break;
        default:                        third = (root + 4) % 12; break;
    }

    if (third != -1) pcs.push_back(third);
    pcs.push_back(fifth);
    if (seventh != -1) pcs.push_back(seventh);

    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    return pcs;
}

static int GetThirdInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
            return 3;
        case ChordQuality::Sus2: return 2;
        case ChordQuality::Sus4: return 5;
        default: return 4;
    }
}

static int GetSeventhInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Dominant7:
        case ChordQuality::Minor7:
        case ChordQuality::HalfDiminished7:
            return 10;
        case ChordQuality::Major7:
            return 11;
        default: return -1;
    }
}

static std::vector<int> GetChordTonesInRange(const ActiveChordContext& chord, int minPitch, int maxPitch) {
    auto pcs = GetPitchClasses(chord);
    std::vector<int> tones;
    for (int p = minPitch; p <= maxPitch; ++p) {
        if (std::find(pcs.begin(), pcs.end(), p % 12) != pcs.end()) {
            tones.push_back(p);
        }
    }
    return tones;
}

static bool IsGuideTone(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);
    int thirdPc = (root + GetThirdInterval(chord.quality)) % 12;
    int seventhInterval = GetSeventhInterval(chord.quality);
    if (pc == thirdPc) return true;
    if (seventhInterval >= 0 && pc == (root + seventhInterval) % 12) return true;
    return false;
}

static bool IsFifth(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);
    int fifthPc;
    switch (chord.quality) {
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
            fifthPc = (root + 6) % 12; break;
        case ChordQuality::Augmented:
            fifthPc = (root + 8) % 12; break;
        default:
            fifthPc = (root + 7) % 12; break;
    }
    return pc == fifthPc;
}

static int GetNinthPc(const ActiveChordContext& chord) {
    return (static_cast<int>(chord.root) + 2) % 12;
}

// ---------------------------------------------------------
// Avoid-Tone Logic
// ---------------------------------------------------------
static bool IsAvoidTone(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);

    switch (chord.quality) {
        case ChordQuality::Major:
        case ChordQuality::Major7:
            if (pc == (root + 1) % 12) return true;
            if (pc == (root + 6) % 12) return true;
            break;
        case ChordQuality::Dominant7:
            if (pc == (root + 1) % 12) return true;
            break;
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
            if (pc == (root + 1) % 12) return true;
            if (pc == (root + 8) % 12) return true;
            break;
        default:
            break;
    }
    return false;
}

// ---------------------------------------------------------
// Shared Pitch Class Count
// ---------------------------------------------------------
static int SharedPitchClasses(const ActiveChordContext& a, const ActiveChordContext& b) {
    auto pcsA = GetPitchClasses(a);
    auto pcsB = GetPitchClasses(b);
    int shared = 0;
    for (int pa : pcsA) {
        for (int pb : pcsB) {
            if (pa == pb) { ++shared; break; }
        }
    }
    return shared;
}

// Does this chord quality contain a tritone? (3rd and b7 present)
static bool HasTritone(ChordQuality quality) {
    return quality == ChordQuality::Dominant7 ||
           quality == ChordQuality::HalfDiminished7;
}

// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------
PianoVoicingPlanner::PianoVoicingPlanner(PianoStyle style, SopranoContour contour)
    : m_style(style), m_contour(contour) {
    switch (style) {
        case PianoStyle::PopBlock:
            m_lhStrategy = LHStrategy::RootFifth;
            m_rhMinPitch = 60;
            m_maxRHSpan = 12;
            m_minVoiceSep = 2;
            m_allowRHInner = true;
            break;
        case PianoStyle::SingerSongwriter:
            m_lhStrategy = LHStrategy::RootTenth;
            m_rhMinPitch = 60;
            m_maxRHSpan = 14;
            m_minVoiceSep = 3;
            m_allowRHInner = true;
            break;
        case PianoStyle::JazzShell:
            m_lhStrategy = LHStrategy::Shell;
            m_rhMinPitch = 55;
            m_maxRHSpan = 12;
            m_minVoiceSep = 2;
            m_allowRHInner = true;
            break;
    }
}

// ---------------------------------------------------------
// Main Entry Point (3-pass)
// ---------------------------------------------------------
std::vector<PianoVoicing> PianoVoicingPlanner::SolveTimeline(
    const std::vector<ChordTrackEvent>& chordTimeline,
    std::vector<VoicingExplanation>* explanations) const {

    std::vector<PianoVoicing> timeline(chordTimeline.size());
    if (chordTimeline.empty()) return timeline;

    if (explanations) {
        explanations->resize(chordTimeline.size());
    }

    auto sopranoPath = PlanSopranoTrajectory(chordTimeline.size());
    SolveOuterVoices(chordTimeline, sopranoPath, timeline);
    SolveInnerVoices(chordTimeline, timeline, explanations);

    return timeline;
}

// ---------------------------------------------------------
// Pass 1: Soprano Trajectory
// ---------------------------------------------------------
std::vector<int> PianoVoicingPlanner::PlanSopranoTrajectory(size_t phraseLen) const {
    std::vector<int> trajectory(phraseLen);

    constexpr int kSopFloor = 65;
    constexpr int kSopCeil  = 79;
    constexpr int kSopMid   = 72;

    for (size_t i = 0; i < phraseLen; ++i) {
        float t = (phraseLen > 1) ? static_cast<float>(i) / static_cast<float>(phraseLen - 1) : 0.5f;

        float target;
        switch (m_contour) {
            case SopranoContour::Hold:
                target = static_cast<float>(kSopMid);
                break;
            case SopranoContour::Rise:
                target = static_cast<float>(kSopFloor) + t * static_cast<float>(kSopCeil - kSopFloor);
                break;
            case SopranoContour::Fall:
                target = static_cast<float>(kSopCeil) - t * static_cast<float>(kSopCeil - kSopFloor);
                break;
            case SopranoContour::Arch:
                target = static_cast<float>(kSopFloor) +
                         (1.0f - 4.0f * (t - 0.5f) * (t - 0.5f)) *
                         static_cast<float>(kSopCeil - kSopFloor);
                break;
        }

        trajectory[i] = static_cast<int>(std::round(target));
    }

    return trajectory;
}

// ---------------------------------------------------------
// Pass 2: Outer Voices
// ---------------------------------------------------------
void PianoVoicingPlanner::SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                                            const std::vector<int>& sopranoPath,
                                            std::vector<PianoVoicing>& t) const {
    int currentSoprano = sopranoPath[0];

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        const auto& ctx = chordTimeline[i].chord;
        int root = static_cast<int>(ctx.root);

        // --- LEFT HAND ---
        int bassNote;
        if (!ctx.isRootPosition()) {
            bassNote = 36 + static_cast<int>(ctx.overBass);
        } else {
            bassNote = 36 + root;
        }
        if (bassNote > 52) bassNote -= 12;
        if (bassNote < 36) bassNote += 12;

        t[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)] = static_cast<uint8_t>(bassNote);

        switch (m_lhStrategy) {
            case LHStrategy::RootFifth: {
                if (bassNote >= 38) {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)] = static_cast<uint8_t>(bassNote + 7);
                }
                break;
            }
            case LHStrategy::RootTenth: {
                t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] =
                    static_cast<uint8_t>(bassNote + 12 + GetThirdInterval(ctx.quality));
                break;
            }
            case LHStrategy::RootOnly:
                break;
            case LHStrategy::Shell: {
                int seventhInterval = GetSeventhInterval(ctx.quality);
                if (seventhInterval >= 0) {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] =
                        static_cast<uint8_t>(bassNote + seventhInterval);
                } else {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] =
                        static_cast<uint8_t>(bassNote + GetThirdInterval(ctx.quality));
                }
                break;
            }
        }

        // --- SOPRANO ---
        auto rhTones = GetChordTonesInRange(ctx, 65, 79);

        int trajectoryTarget = sopranoPath[i];
        int bestTone = currentSoprano;
        float bestCost = 99999.0f;

        for (int tone : rhTones) {
            float trajCost = std::abs(static_cast<float>(tone) - static_cast<float>(trajectoryTarget)) * 0.6f;
            float smoothCost = static_cast<float>(std::abs(tone - currentSoprano));
            float totalCost = trajCost + smoothCost;

            if (tone == currentSoprano) totalCost -= 0.5f;
            if (IsGuideTone(ctx, tone % 12)) totalCost -= 0.3f;

            int trajDelta = trajectoryTarget - currentSoprano;
            int toneDelta = tone - currentSoprano;
            if (trajDelta != 0 && toneDelta != 0) {
                bool sameDir = (trajDelta > 0 && toneDelta > 0) || (trajDelta < 0 && toneDelta < 0);
                if (sameDir) totalCost -= 0.25f;
            }

            if (totalCost < bestCost) {
                bestCost = totalCost;
                bestTone = tone;
            }
        }

        t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)] = static_cast<uint8_t>(bestTone);
        currentSoprano = bestTone;
    }
}

// ---------------------------------------------------------
// Chord Obligation (Style-Conditioned)
// ---------------------------------------------------------

static ChordObligation GetBaseObligation(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major:       return {{4}, {0}, 3};
        case ChordQuality::Minor:       return {{3}, {0}, 3};
        case ChordQuality::Major7:      return {{4, 11}, {0}, 3};
        case ChordQuality::Minor7:      return {{3, 10}, {0}, 3};
        case ChordQuality::Dominant7:   return {{4, 10}, {0}, 3};
        case ChordQuality::HalfDiminished7: return {{3, 6, 10}, {}, 3};
        case ChordQuality::Diminished:  return {{3, 6}, {0}, 3};
        case ChordQuality::Augmented:   return {{4, 8}, {}, 3};
        case ChordQuality::Sus4:        return {{5}, {0}, 3};
        case ChordQuality::Sus2:        return {{2}, {0}, 3};
        case ChordQuality::Add9:        return {{4, 2}, {0}, 3};
        case ChordQuality::PowerChord:  return {{}, {7}, 2};
        default:                        return {{4}, {0}, 3};
    }
}

static ChordObligation GetChordObligation(ChordQuality quality, PianoStyle style) {
    auto ob = GetBaseObligation(quality);

    switch (style) {
        case PianoStyle::JazzShell:
            ob.preferred.clear();
            ob.maxDesiredDensity = 2;
            break;
        case PianoStyle::PopBlock:
            if (quality == ChordQuality::Major || quality == ChordQuality::Minor) {
                ob.preferred.push_back(7);
            }
            ob.maxDesiredDensity = 3;
            break;
        case PianoStyle::SingerSongwriter:
            if (quality == ChordQuality::Major || quality == ChordQuality::Minor ||
                quality == ChordQuality::Major7 || quality == ChordQuality::Minor7 ||
                quality == ChordQuality::Dominant7) {
                ob.preferred.push_back(2);
            }
            ob.maxDesiredDensity = 3;
            break;
    }

    return ob;
}

// ---------------------------------------------------------
// Color Tone Interval
// ---------------------------------------------------------
static int GetColorToneInterval(PianoStyle style, ChordQuality quality) {
    switch (style) {
        case PianoStyle::PopBlock:
            switch (quality) {
                case ChordQuality::Major:
                case ChordQuality::Major7:
                case ChordQuality::Minor:
                case ChordQuality::Minor7:
                    return 7;
                case ChordQuality::Dominant7:
                    return 2;
                case ChordQuality::Sus2:
                case ChordQuality::Sus4:
                case ChordQuality::Diminished:
                case ChordQuality::HalfDiminished7:
                    return -1;
                default:
                    return 7;
            }
        case PianoStyle::SingerSongwriter:
            switch (quality) {
                case ChordQuality::Major:
                case ChordQuality::Major7:
                case ChordQuality::Minor:
                case ChordQuality::Minor7:
                case ChordQuality::Dominant7:
                    return 2;
                case ChordQuality::Sus4:
                    return 2;
                case ChordQuality::Sus2:
                    return -1;
                default:
                    return -1;
            }
        case PianoStyle::JazzShell:
            switch (quality) {
                case ChordQuality::Major7:
                case ChordQuality::Minor:
                case ChordQuality::Major:
                    return 2;
                case ChordQuality::Minor7:
                case ChordQuality::HalfDiminished7:
                    return 5;
                case ChordQuality::Dominant7:
                    return 9;
                default:
                    return -1;
            }
    }
    return -1;
}

// ---------------------------------------------------------
// Voice Leading Cost
// ---------------------------------------------------------
static float VoiceLeadingCost(int prev, int curr) {
    if (prev == 0 || curr == 0) return 0.0f;
    int dist = std::abs(curr - prev);
    if (dist == 0) return 0.0f;
    if (dist == 1) return 1.0f;
    if (dist == 2) return 2.0f;
    return 2.0f + static_cast<float>(dist - 2) * 1.5f;
}

// Minimum-cost assignment over 3 slots (3! = 6 permutations).
static float MinCostAssignment(const int prev[3], const int curr[3]) {
    static constexpr int perms[6][3] = {
        {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}
    };

    float bestCost = 99999.0f;
    for (const auto& p : perms) {
        float cost = 0.0f;
        for (int i = 0; i < 3; ++i) {
            cost += VoiceLeadingCost(prev[p[i]], curr[i]);
        }
        if (cost < bestCost) bestCost = cost;
    }
    return bestCost;
}

// =========================================================
// EXTRACTED EVALUATOR 1: ClassifyTransition
// =========================================================
// Determines the harmonic function between consecutive chords
// and sets continuity weight + sufficiency gate accordingly.

static TransitionContext ClassifyTransition(
    const ActiveChordContext& prev, const PianoVoicing& prevVoicing,
    const ActiveChordContext& curr, const PianoVoicing& currVoicing) {

    TransitionContext ctx;
    ctx.sharedPitchClasses = SharedPitchClasses(prev, curr);

    int prevBassPc = prevVoicing.GetPitch(PianoTargetRole::LH_Root) % 12;
    int currBassPc = currVoicing.GetPitch(PianoTargetRole::LH_Root) % 12;

    int prevRoot = static_cast<int>(prev.root);
    int currRoot = static_cast<int>(curr.root);

    // --- Pedal: same bass, different harmony ---
    bool isPedal = (currBassPc == prevBassPc) &&
                   (curr.root != prev.root || curr.quality != prev.quality);

    // --- Dominant Resolution: root descends P5, previous has tritone ---
    // V→I: the previous root is a P5 above the current root.
    // Example: G7→C (G=7, C=0, (0+7)%12 == 7 ✓)
    bool isDomRes = HasTritone(prev.quality) &&
                    ((currRoot + 7) % 12 == prevRoot);

    // --- Reinterpretive Hold: 3+ shared PCs, root changes ---
    bool isReinterp = (ctx.sharedPitchClasses >= 3) && (curr.root != prev.root);

    // Priority: DominantResolution > Pedal > ReinterpretiveHold > Default
    // A dominant resolution over a pedal bass is still a dominant resolution.
    if (isDomRes) {
        ctx.function = HarmonicFunction::DominantResolution;
        ctx.sufficiencyGate = 100; // full obligation — must spell the resolution target
        ctx.continuityWeight = 0.6f; // expect motion, not holds
    } else if (isPedal) {
        ctx.function = HarmonicFunction::Pedal;
        ctx.sufficiencyGate = 50;
        ctx.continuityWeight = 1.0f;
    } else if (isReinterp) {
        ctx.function = HarmonicFunction::ReinterpretiveHold;
        ctx.sufficiencyGate = 50;
        ctx.continuityWeight = 1.0f;
    } else {
        ctx.function = HarmonicFunction::Default;
        ctx.sufficiencyGate = 100;
        // Scale by shared PCs
        if (ctx.sharedPitchClasses == 0) ctx.continuityWeight = 0.3f;
        else if (ctx.sharedPitchClasses == 1) ctx.continuityWeight = 0.5f;
        else if (ctx.sharedPitchClasses == 2) ctx.continuityWeight = 0.8f;
        else ctx.continuityWeight = 1.0f;
    }

    return ctx;
}

// =========================================================
// EXTRACTED EVALUATOR 2: ComputeObligationCoverage
// =========================================================
// Pre-computes which required tones are already covered by
// outer voices, and which remain for the inner voice tuple.

static ObligationCoverage ComputeObligationCoverage(
    const ActiveChordContext& chord, PianoStyle style,
    const PianoVoicing& voicing) {

    ObligationCoverage cov;
    cov.obligation = GetChordObligation(chord.quality, style);
    int root = static_cast<int>(chord.root);

    int topPc = voicing.GetPitch(PianoTargetRole::RH_Top) % 12;
    int bassPc = voicing.GetPitch(PianoTargetRole::LH_Root) % 12;

    cov.coveredPcs.push_back(topPc);
    if (bassPc != topPc) cov.coveredPcs.push_back(bassPc);

    uint8_t lh2 = voicing.GetPitch(PianoTargetRole::LH_Fifth);
    if (lh2 == 0) lh2 = voicing.GetPitch(PianoTargetRole::LH_ShellLow);
    if (lh2 > 0) {
        int lh2pc = lh2 % 12;
        if (std::find(cov.coveredPcs.begin(), cov.coveredPcs.end(), lh2pc) == cov.coveredPcs.end()) {
            cov.coveredPcs.push_back(lh2pc);
        }
    }

    for (int interval : cov.obligation.required) {
        int reqPc = (root + interval) % 12;
        if (std::find(cov.coveredPcs.begin(), cov.coveredPcs.end(), reqPc) == cov.coveredPcs.end()) {
            cov.unmetRequired.push_back(reqPc);
        }
    }

    for (int interval : cov.obligation.preferred) {
        int prefPc = (root + interval) % 12;
        if (std::find(cov.coveredPcs.begin(), cov.coveredPcs.end(), prefPc) == cov.coveredPcs.end()) {
            cov.unmetPreferred.push_back(prefPc);
        }
    }

    return cov;
}

// =========================================================
// EXTRACTED EVALUATOR 3: EvaluateTuple
// =========================================================
// Scores a single (gh, gl, in) tuple across all three tiers.
// Returns the TieredScore for lexicographic comparison.

static TieredScore EvaluateTuple(
    int gh, int gl, int in,
    const ObligationCoverage& cov,
    const TransitionContext& tctx,
    const int prevVoices[3]) {

    // Collect pitch classes provided by this tuple
    std::vector<int> tuplePcs;
    if (gh > 0) tuplePcs.push_back(gh % 12);
    if (gl > 0) tuplePcs.push_back(gl % 12);
    if (in > 0) tuplePcs.push_back(in % 12);

    int density = (gh > 0 ? 1 : 0) + (gl > 0 ? 1 : 0) + (in > 0 ? 1 : 0);

    // --- Tier 1: Harmonic sufficiency ---
    int metRequired = 0;
    for (int reqPc : cov.unmetRequired) {
        if (std::find(tuplePcs.begin(), tuplePcs.end(), reqPc) != tuplePcs.end()) {
            metRequired++;
        }
    }

    int harmonic = 0;
    bool sufficient = (metRequired == static_cast<int>(cov.unmetRequired.size()));
    if (sufficient) {
        harmonic = tctx.sufficiencyGate;

        // Preferred tone bonus
        for (int prefPc : cov.unmetPreferred) {
            if (std::find(tuplePcs.begin(), tuplePcs.end(), prefPc) != tuplePcs.end()) {
                harmonic += 10;
            }
        }

        // Register-awareness: penalize required tones buried at the bottom
        if (density >= 2) {
            int pitches[3] = {gh, gl, in};
            int lo1 = 999, lo2 = 999;
            for (int p : pitches) {
                if (p > 0 && p < lo1) { lo2 = lo1; lo1 = p; }
                else if (p > 0 && p < lo2) { lo2 = p; }
            }
            if (lo1 < 999 && lo2 < 999 && (lo2 - lo1) > 4) {
                int lo1pc = lo1 % 12;
                for (int reqPc : cov.unmetRequired) {
                    if (reqPc == lo1pc) {
                        harmonic -= 5;
                        break;
                    }
                }
            }
        }
    } else {
        harmonic = metRequired * 20;
    }

    // --- Tier 2: Continuity ---
    float continuity = 0.0f;
    if (prevVoices[0] > 0 || prevVoices[1] > 0 || prevVoices[2] > 0) {
        int curr[3] = {gh, gl, in};
        float vlCost = MinCostAssignment(prevVoices, curr);
        continuity = -vlCost * tctx.continuityWeight;
    }

    // --- Tier 3: Density (style-conditioned economy) ---
    int densityScore = 0;
    if (density <= cov.obligation.maxDesiredDensity) {
        densityScore = density;
    } else {
        densityScore = cov.obligation.maxDesiredDensity - (density - cov.obligation.maxDesiredDensity);
    }

    return {harmonic, continuity, densityScore};
}

// =========================================================
// EXPLANATION BUILDER
// =========================================================
// Populates a VoicingExplanation from obligation coverage,
// transition context, and the chosen tuple.

static void BuildExplanation(
    VoicingExplanation& exp,
    const ActiveChordContext& chord,
    const PianoVoicing& voicing,
    const ObligationCoverage& cov,
    const TransitionContext& tctx,
    int candidatesEvaluated, int candidatesRejected) {

    int root = static_cast<int>(chord.root);
    int topPc = voicing.GetPitch(PianoTargetRole::RH_Top) % 12;
    int bassPc = voicing.GetPitch(PianoTargetRole::LH_Root) % 12;

    uint8_t lh2 = voicing.GetPitch(PianoTargetRole::LH_Fifth);
    if (lh2 == 0) lh2 = voicing.GetPitch(PianoTargetRole::LH_ShellLow);
    int lh2pc = (lh2 > 0) ? (lh2 % 12) : -1;

    // Inner voice PCs
    std::vector<int> innerPcs;
    uint8_t gh = voicing.GetPitch(PianoTargetRole::RH_GuideHigh);
    uint8_t gl = voicing.GetPitch(PianoTargetRole::RH_GuideLow);
    uint8_t in = voicing.GetPitch(PianoTargetRole::RH_Inner);
    if (gh > 0) innerPcs.push_back(gh % 12);
    if (gl > 0) innerPcs.push_back(gl % 12);
    if (in > 0) innerPcs.push_back(in % 12);

    exp.totalRequired = static_cast<int>(cov.obligation.required.size());
    exp.coveredBySoprano = 0;
    exp.coveredByBass = 0;
    exp.coveredByLh2 = 0;
    exp.suppliedByTuple = 0;
    exp.unmet = 0;

    for (int interval : cov.obligation.required) {
        int reqPc = (root + interval) % 12;
        if (topPc == reqPc) exp.coveredBySoprano++;
        else if (bassPc == reqPc) exp.coveredByBass++;
        else if (lh2pc == reqPc) exp.coveredByLh2++;
        else if (std::find(innerPcs.begin(), innerPcs.end(), reqPc) != innerPcs.end()) exp.suppliedByTuple++;
        else exp.unmet++;
    }

    exp.sufficient = (exp.unmet == 0);
    exp.transitionFunction = tctx.function;
    exp.sufficiencyGateUsed = tctx.sufficiencyGate;
    exp.continuityWeightUsed = tctx.continuityWeight;
    exp.candidatesEvaluated = candidatesEvaluated;
    exp.candidatesRejected = candidatesRejected;
}

// =========================================================
// Pass 3: SolveInnerVoices (Orchestrator)
// =========================================================
// Now a thin loop that delegates to the extracted evaluators.

void PianoVoicingPlanner::SolveInnerVoices(
    const std::vector<ChordTrackEvent>& chordTimeline,
    std::vector<PianoVoicing>& t,
    std::vector<VoicingExplanation>* explanations) const {

    int prevVoices[3] = {0, 0, 0};

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int topPitch = t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        const auto& ctx = chordTimeline[i].chord;
        int topPc = topPitch % 12;

        // --- Classify transition ---
        TransitionContext tctx;
        if (i > 0) {
            tctx = ClassifyTransition(
                chordTimeline[i - 1].chord, t[i - 1],
                ctx, t[i]);
        }

        // --- Compute obligation coverage ---
        ObligationCoverage cov = ComputeObligationCoverage(ctx, m_style, t[i]);

        // --- Prepare candidates ---
        int searchFloor = std::max(m_rhMinPitch, topPitch - m_maxRHSpan);
        auto allTones = GetChordTonesInRange(ctx, searchFloor, topPitch - 1);

        std::vector<int> tones;
        for (int tone : allTones) {
            if (tone % 12 != topPc) tones.push_back(tone);
        }

        // Color tone candidates
        int colorPc = -1;
        if (m_allowRHInner) {
            int colorInterval = GetColorToneInterval(m_style, ctx.quality);
            if (colorInterval >= 0) {
                int cpCandidate = (static_cast<int>(ctx.root) + colorInterval) % 12;
                if (!IsAvoidTone(ctx, cpCandidate) && cpCandidate != topPc) {
                    colorPc = cpCandidate;
                }
            }
        }

        std::vector<int> colorTones;
        if (colorPc >= 0) {
            for (int p = searchFloor; p < topPitch; ++p) {
                if (p % 12 == colorPc && p >= m_rhMinPitch) {
                    colorTones.push_back(p);
                }
            }
        }

        // --- Enumerate and score tuples ---
        TieredScore bestScore = {-9999, -99999.0f, -1};
        struct Tuple { int gh, gl, inner; TieredScore score; };
        Tuple bestTuple = {0, 0, 0, bestScore};
        int evaluated = 0, rejected = 0;

        std::vector<int> ghCandidates = {0};
        for (int tone : tones) ghCandidates.push_back(tone);

        for (int gh : ghCandidates) {
            if (gh > 0 && topPitch - gh < m_minVoiceSep) continue;

            std::vector<int> glCandidates = {0};
            for (int tone : tones) {
                if (tone == gh) continue;
                if (gh > 0 && tone % 12 == gh % 12) continue;
                if (gh > 0 && gh - tone < m_minVoiceSep) continue;
                if (gh == 0 && topPitch - tone < m_minVoiceSep) continue;
                glCandidates.push_back(tone);
            }

            for (int gl : glCandidates) {
                if (gh > 0 && gl > 0 && gl >= gh) continue;

                std::vector<int> inCandidates = {0};
                for (int ct : colorTones) {
                    if (ct == gh || ct == gl) continue;
                    if (gh > 0 && ct % 12 == gh % 12) continue;
                    if (gl > 0 && ct % 12 == gl % 12) continue;
                    if (ct % 12 == topPc) continue;
                    bool tooClose = false;
                    if (gh > 0 && std::abs(ct - gh) < m_minVoiceSep) tooClose = true;
                    if (gl > 0 && std::abs(ct - gl) < m_minVoiceSep) tooClose = true;
                    if (std::abs(ct - topPitch) < m_minVoiceSep) tooClose = true;
                    if (tooClose) continue;
                    inCandidates.push_back(ct);
                }

                for (int in : inCandidates) {
                    // Hard gate: span
                    int lowest = topPitch;
                    if (gh > 0 && gh < lowest) lowest = gh;
                    if (gl > 0 && gl < lowest) lowest = gl;
                    if (in > 0 && in < lowest) lowest = in;
                    if (topPitch - lowest > m_maxRHSpan) { rejected++; continue; }

                    evaluated++;
                    TieredScore ts = EvaluateTuple(gh, gl, in, cov, tctx, prevVoices);
                    if (ts > bestScore) {
                        bestScore = ts;
                        bestTuple = {gh, gl, in, ts};
                    }
                }
            }
        }

        // --- Assign ---
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = static_cast<uint8_t>(bestTuple.gh);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)]  = static_cast<uint8_t>(bestTuple.gl);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_Inner)]     = static_cast<uint8_t>(bestTuple.inner);

        // --- Explanation ---
        if (explanations) {
            BuildExplanation((*explanations)[i], ctx, t[i], cov, tctx,
                             evaluated, rejected);
        }

        prevVoices[0] = bestTuple.gh;
        prevVoices[1] = bestTuple.gl;
        prevVoices[2] = bestTuple.inner;
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
