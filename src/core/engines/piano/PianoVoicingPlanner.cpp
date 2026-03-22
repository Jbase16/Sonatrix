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
// Certain pitch classes create harsh dissonances in specific
// chord contexts. These are not "wrong notes" — they are
// context-dependent clashes that a competent pianist would avoid.
static bool IsAvoidTone(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);

    switch (chord.quality) {
        case ChordQuality::Major:
        case ChordQuality::Major7:
            // b9 (minor 2nd from root) clashes with major 3rd
            if (pc == (root + 1) % 12) return true;
            // #11 (tritone from root) clashes in pop context unless Lydian
            if (pc == (root + 6) % 12) return true;
            break;
        case ChordQuality::Dominant7:
            // b9 is sometimes used but generally avoided in pop
            if (pc == (root + 1) % 12) return true;
            break;
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
            // b2 (minor 9th from root) clashes with minor 3rd (Phrygian sound)
            if (pc == (root + 1) % 12) return true;
            // b6 (minor 6th) is context-dependent; avoid in pop, fine in Dorian
            if (pc == (root + 8) % 12) return true;
            break;
        default:
            break;
    }
    return false;
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
std::vector<PianoVoicing> PianoVoicingPlanner::SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) const {
    std::vector<PianoVoicing> timeline(chordTimeline.size());
    if (chordTimeline.empty()) return timeline;

    // Pass 1: Plan the soprano trajectory independent of harmony
    auto sopranoPath = PlanSopranoTrajectory(chordTimeline.size());

    // Pass 2: Solve outer voices (snap trajectory to chord tones + LH)
    SolveOuterVoices(chordTimeline, sopranoPath, timeline);

    // Pass 3: Inner voice fill + color tone
    SolveInnerVoices(chordTimeline, timeline);

    return timeline;
}

// ---------------------------------------------------------
// Pass 1: Pre-compute the ideal Soprano trajectory
// ---------------------------------------------------------
// This is the key structural difference from Phase 19.2.
// Instead of choosing chord tones locally, we first define what
// the top-line SHOULD do across the phrase, then constrain
// chord-tone selection to serve that shape.
//
// The trajectory emits a concrete MIDI pitch target per position.
// It does NOT yet know what chord tones are available — that
// happens in Pass 2 when we snap to the nearest valid tone.
std::vector<int> PianoVoicingPlanner::PlanSopranoTrajectory(size_t phraseLen) const {
    std::vector<int> trajectory(phraseLen);

    // Define soprano register endpoints
    constexpr int kSopFloor = 65; // F4
    constexpr int kSopCeil  = 79; // G5
    constexpr int kSopMid   = 72; // C5

    for (size_t i = 0; i < phraseLen; ++i) {
        float t = (phraseLen > 1) ? static_cast<float>(i) / static_cast<float>(phraseLen - 1) : 0.5f;

        float target;
        switch (m_contour) {
            case SopranoContour::Hold:
                target = static_cast<float>(kSopMid);
                break;
            case SopranoContour::Rise:
                // Linear ascent from floor to ceiling
                target = static_cast<float>(kSopFloor) + t * static_cast<float>(kSopCeil - kSopFloor);
                break;
            case SopranoContour::Fall:
                // Linear descent from ceiling to floor
                target = static_cast<float>(kSopCeil) - t * static_cast<float>(kSopCeil - kSopFloor);
                break;
            case SopranoContour::Arch:
                // Parabolic: peaks at midpoint
                // f(t) = floor + (1 - 4*(t-0.5)^2) * range
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
// Pass 2: Solve Outer Voices (Bass + Soprano → Trajectory)
// ---------------------------------------------------------
void PianoVoicingPlanner::SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                                            const std::vector<int>& sopranoPath,
                                            std::vector<PianoVoicing>& t) const {
    int currentSoprano = sopranoPath[0]; // seed from trajectory

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

        // --- SOPRANO: Two competing forces ---
        // 1. The planned trajectory (where the contour wants us to be)
        // 2. Local smoothness (distance from current position)
        //
        // The trajectory is the INTENT. Smoothness is the CONSTRAINT.
        // We score each candidate against both, with the trajectory
        // weighted more heavily than in Phase 19.2's pure-gravity approach.

        auto rhTones = GetChordTonesInRange(ctx, 65, 79);

        int trajectoryTarget = sopranoPath[i];
        int bestTone = currentSoprano;
        float bestCost = 99999.0f;

        for (int tone : rhTones) {
            // Cost 1: Distance from planned trajectory target
            // This is the primary shaping force. Weight = 0.6
            float trajCost = std::abs(static_cast<float>(tone) - static_cast<float>(trajectoryTarget)) * 0.6f;

            // Cost 2: Distance from current position (smoothness)
            // This prevents wild jumps. Weight = 1.0
            float smoothCost = static_cast<float>(std::abs(tone - currentSoprano));

            float totalCost = trajCost + smoothCost;

            // Common tone reward (holding a pitch is desirable unless the
            // trajectory strongly wants us to move)
            if (tone == currentSoprano) {
                totalCost -= 0.5f;
            }

            // Guide tone preference
            if (IsGuideTone(ctx, tone % 12)) {
                totalCost -= 0.3f;
            }

            // Directional momentum: if the trajectory is asking for movement,
            // reward motion in the right direction
            int trajDelta = trajectoryTarget - currentSoprano;
            int toneDelta = tone - currentSoprano;
            if (trajDelta != 0 && toneDelta != 0) {
                // Same sign = moving in trajectory direction
                bool sameDir = (trajDelta > 0 && toneDelta > 0) || (trajDelta < 0 && toneDelta < 0);
                if (sameDir) {
                    totalCost -= 0.25f;
                }
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
// Chord Obligation Model (Style-Conditioned)
// ---------------------------------------------------------
// Defines per (quality × style) which intervals are required/preferred,
// and the maximum desired inner voice density. When density exceeds
// maxDesiredDensity, the density tier becomes a penalty instead of
// a bonus — this encodes economy preference.

struct ChordObligation {
    std::vector<int> required;     // intervals that must appear for harmonic identity
    std::vector<int> preferred;    // intervals that are nice to have (tiebreaker)
    int maxDesiredDensity;         // inner voices beyond this count are penalized
};

// Base obligation by quality (style-independent structural requirements)
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

// Style overlay: modifies base obligation
static ChordObligation GetChordObligation(ChordQuality quality, PianoStyle style) {
    auto ob = GetBaseObligation(quality);

    switch (style) {
        case PianoStyle::JazzShell:
            // Jazz: economy is the ethos. Guide tones only.
            // Root is almost never needed in RH (bass has it).
            // Prefer lean voicings: max 2 inner voices.
            ob.preferred.clear(); // no root preference
            ob.maxDesiredDensity = 2;
            break;

        case PianoStyle::PopBlock:
            // Pop: fuller voicings welcome. Root doubling acceptable.
            // 5th is sometimes preferred on triads for fullness.
            if (quality == ChordQuality::Major || quality == ChordQuality::Minor) {
                ob.preferred.push_back(7); // 5th preferred for triads
            }
            ob.maxDesiredDensity = 3;
            break;

        case PianoStyle::SingerSongwriter:
            // SS: moderate density. 9th preferred as color extension.
            if (quality == ChordQuality::Major || quality == ChordQuality::Minor ||
                quality == ChordQuality::Major7 || quality == ChordQuality::Minor7 ||
                quality == ChordQuality::Dominant7) {
                ob.preferred.push_back(2); // 9th as color
            }
            ob.maxDesiredDensity = 3;
            break;
    }

    return ob;
}

// ---------------------------------------------------------
// Pass 3: Joint Inner Voice Selection with Cross-Role Assignment
// ---------------------------------------------------------
// Evaluates (gh, gl, inner) tuples jointly with tiered scoring:
//   Hard gates: spacing, span, duplicate PC, mud register
//   Tier 1 (Harmonic): sufficiency gate (all unmet required tones provided?)
//           + preferred tone bonus. Sufficient tuples always dominate.
//   Tier 2 (Continuity): cross-role minimum-cost assignment vs previous voicing
//   Tier 3 (Density): populated seat count (last tiebreaker)

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

// Compute the cost of a single voice transition.
// Lower is better. 0 = hold, 1.0 = semitone, scaling up.
static float VoiceLeadingCost(int prev, int curr) {
    if (prev == 0 || curr == 0) return 0.0f; // no prior = no cost
    int dist = std::abs(curr - prev);
    if (dist == 0) return 0.0f;
    if (dist == 1) return 1.0f;
    if (dist == 2) return 2.0f;
    return 2.0f + static_cast<float>(dist - 2) * 1.5f;
}

// Minimum-cost assignment over 3 slots.
// prev[3] and curr[3] are the pitches to match.
// Returns the minimum total voice-leading cost across all
// valid permutations of mapping prev->curr.
static float MinCostAssignment(const int prev[3], const int curr[3]) {
    // 3! = 6 permutations. Enumerate all.
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

// Count shared pitch classes between two chords (0-4 typically).
// Used to scale the continuity bonus: more shared PCs = stronger hold preference.
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

void PianoVoicingPlanner::SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const {
    int prevVoices[3] = {0, 0, 0}; // {GuideHigh, GuideLow, Inner}

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int topPitch = t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        const auto& ctx = chordTimeline[i].chord;
        int topPc = topPitch % 12;

        int searchFloor = std::max(m_rhMinPitch, topPitch - m_maxRHSpan);
        auto allTones = GetChordTonesInRange(ctx, searchFloor, topPitch - 1);

        // Filter: remove tones that duplicate soprano pitch class
        std::vector<int> tones;
        for (int t : allTones) {
            if (t % 12 != topPc) tones.push_back(t);
        }

        // Determine available color tone pitch class
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

        // Find valid color tone pitches
        std::vector<int> colorTones;
        if (colorPc >= 0) {
            for (int p = searchFloor; p < topPitch; ++p) {
                if (p % 12 == colorPc && p >= m_rhMinPitch) {
                    colorTones.push_back(p);
                }
            }
        }

        // Context-dependent continuity weight.
        // More shared pitch classes = higher weight (the hold is harmonically justified).
        // Remote modulations get lower weight (hold might be coincidental, not intentional).
        float continuityWeight = 1.0f;
        if (i > 0) {
            int shared = SharedPitchClasses(chordTimeline[i - 1].chord, ctx);
            // 0 shared = remote modulation, weight 0.3
            // 1 shared = distant, weight 0.5
            // 2 shared = related, weight 0.8
            // 3+ shared = close, weight 1.0
            if (shared == 0) continuityWeight = 0.3f;
            else if (shared == 1) continuityWeight = 0.5f;
            else if (shared == 2) continuityWeight = 0.8f;
            else continuityWeight = 1.0f;
        }

        // --- Chord obligation analysis ---
        // Determine which required tones are already covered by soprano + bass.
        const auto obligation = GetChordObligation(ctx.quality, m_style);
        int root = static_cast<int>(ctx.root);
        int bassPc = t[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)] % 12;

        // Pitch classes already covered by outer voices
        std::vector<int> coveredPcs;
        coveredPcs.push_back(topPc);
        if (bassPc != topPc) coveredPcs.push_back(bassPc);
        // Also add LH second voice if present
        uint8_t lh2 = t[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)];
        if (lh2 == 0) lh2 = t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)];
        if (lh2 > 0) {
            int lh2pc = lh2 % 12;
            if (std::find(coveredPcs.begin(), coveredPcs.end(), lh2pc) == coveredPcs.end()) {
                coveredPcs.push_back(lh2pc);
            }
        }

        // Which required intervals are still unmet?
        std::vector<int> unmetRequired;
        for (int interval : obligation.required) {
            int requiredPc = (root + interval) % 12;
            if (std::find(coveredPcs.begin(), coveredPcs.end(), requiredPc) == coveredPcs.end()) {
                unmetRequired.push_back(requiredPc);
            }
        }

        // Which preferred intervals are still unmet?
        std::vector<int> unmetPreferred;
        for (int interval : obligation.preferred) {
            int prefPc = (root + interval) % 12;
            if (std::find(coveredPcs.begin(), coveredPcs.end(), prefPc) == coveredPcs.end()) {
                unmetPreferred.push_back(prefPc);
            }
        }

        // --- Function-sensitive obligation relaxation ---
        // In certain harmonic contexts, the sufficiency gate is relaxed:
        // strong continuity can compete with incomplete harmonic spelling.
        //
        // Pedal bass: LH root unchanged from previous chord, but harmony changed.
        //   The listener's ear anchors to the pedal; upper voice continuity matters more.
        // Close relation: 3+ shared PCs with previous chord.
        //   The harmonic shift is mild; implied tones are acceptable.
        int sufficiencyGate = 100; // default: sufficient always dominates
        if (i > 0) {
            int prevBassPc = t[i - 1].pitches[static_cast<int>(PianoTargetRole::LH_Root)] % 12;
            bool isPedal = (bassPc == prevBassPc) &&
                           (ctx.root != chordTimeline[i - 1].chord.root ||
                            ctx.quality != chordTimeline[i - 1].chord.quality);
            bool isCloseRelation = (SharedPitchClasses(chordTimeline[i - 1].chord, ctx) >= 3);

            if (isPedal || isCloseRelation) {
                // Relax: sufficient = 50, so a tuple with strong continuity
                // (e.g., -5.0 vlCost) at harmonic=0 can still beat a sufficient
                // tuple with weak continuity at harmonic=50.
                sufficiencyGate = 50;
            }
        }

        // --- Generate and score candidate tuples ---
        // Tier 1 (Harmonic): sufficient (all unmet required met) = sufficiencyGate
        //   + partial required count for insufficient tuples
        //   + preferred bonus (+10 each) within sufficient tuples
        // Tier 2 (Continuity): cross-role voice-leading cost
        // Tier 3 (Density): economy-conditioned seat count

        struct TieredScore {
            int harmonic;      // sufficiency gate + preferred bonus
            float continuity;  // voice-leading cost (negated: higher = better)
            int density;       // populated seats (final tiebreaker)

            bool operator>(const TieredScore& o) const {
                if (harmonic != o.harmonic) return harmonic > o.harmonic;
                if (continuity != o.continuity) return continuity > o.continuity;
                return density > o.density;
            }
        };

        struct Tuple {
            int gh, gl, inner;
            TieredScore score;
        };

        TieredScore bestScore = {-9999, -99999.0f, -1};
        Tuple bestTuple = {0, 0, 0, bestScore};

        // Enumerate GuideHigh candidates (including 0 = skip)
        std::vector<int> ghCandidates = {0};
        for (int t : tones) ghCandidates.push_back(t);

        for (int gh : ghCandidates) {
            // Hard gate: GuideHigh must be below soprano by minVoiceSep
            if (gh > 0 && topPitch - gh < m_minVoiceSep) continue;

            // GuideLow candidates
            std::vector<int> glCandidates = {0};
            for (int t : tones) {
                // Hard gates
                if (t == gh) continue;
                if (gh > 0 && t % 12 == gh % 12) continue;
                if (gh > 0 && gh - t < m_minVoiceSep) continue;
                if (gh == 0 && topPitch - t < m_minVoiceSep) continue;
                glCandidates.push_back(t);
            }

            for (int gl : glCandidates) {
                // Hard gate: ordering
                if (gh > 0 && gl > 0 && gl >= gh) continue;

                // Inner candidates
                std::vector<int> inCandidates = {0};
                for (int ct : colorTones) {
                    // Hard gates
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
                    if (topPitch - lowest > m_maxRHSpan) continue;

                    // --- Tier 1: Harmonic sufficiency ---
                    // Collect pitch classes provided by this tuple
                    std::vector<int> tuplePcs;
                    if (gh > 0) tuplePcs.push_back(gh % 12);
                    if (gl > 0) tuplePcs.push_back(gl % 12);
                    if (in > 0) tuplePcs.push_back(in % 12);

                    // Count populated seats (needed for register check and Tier 3)
                    int density = (gh > 0 ? 1 : 0) + (gl > 0 ? 1 : 0) + (in > 0 ? 1 : 0);

                    // Count how many unmet required tones this tuple satisfies
                    int metRequired = 0;
                    for (int reqPc : unmetRequired) {
                        if (std::find(tuplePcs.begin(), tuplePcs.end(), reqPc) != tuplePcs.end()) {
                            metRequired++;
                        }
                    }

                    int harmonic = 0;
                    bool sufficient = (metRequired == static_cast<int>(unmetRequired.size()));
                    if (sufficient) {
                        harmonic = sufficiencyGate;

                        // Preferred tone bonus within sufficient tuples
                        for (int prefPc : unmetPreferred) {
                            if (std::find(tuplePcs.begin(), tuplePcs.end(), prefPc) != tuplePcs.end()) {
                                harmonic += 10;
                            }
                        }

                        // Register-awareness: penalize required tones buried
                        // at the bottom of a dense voicing. A required tone placed
                        // as the lowest voice, >4 semitones below the next voice,
                        // is perceptually masked.
                        if (density >= 2) {
                            // Find lowest and second-lowest tuple pitches
                            int pitches[3] = {gh, gl, in};
                            int lo1 = 999, lo2 = 999;
                            for (int p : pitches) {
                                if (p > 0 && p < lo1) { lo2 = lo1; lo1 = p; }
                                else if (p > 0 && p < lo2) { lo2 = p; }
                            }
                            if (lo1 < 999 && lo2 < 999 && (lo2 - lo1) > 4) {
                                // Check if lo1 is a required tone
                                int lo1pc = lo1 % 12;
                                for (int reqPc : unmetRequired) {
                                    if (reqPc == lo1pc) {
                                        harmonic -= 5; // buried required tone penalty
                                        break;
                                    }
                                }
                            }
                        }
                    } else {
                        // Partial credit: count of met required obligations
                        // Stays well below sufficiencyGate
                        harmonic = metRequired * 20;
                    }

                    // --- Tier 2: Continuity ---
                    float continuity = 0.0f;
                    if (prevVoices[0] > 0 || prevVoices[1] > 0 || prevVoices[2] > 0) {
                        int curr[3] = {gh, gl, in};
                        float vlCost = MinCostAssignment(prevVoices, curr);
                        continuity = -vlCost * continuityWeight;
                    }

                    // --- Tier 3: Density (style-conditioned economy) ---
                    int densityScore = 0;
                    if (density <= obligation.maxDesiredDensity) {
                        densityScore = density; // more is better, up to threshold
                    } else {
                        // Economy penalty: each voice beyond threshold costs points
                        densityScore = obligation.maxDesiredDensity - (density - obligation.maxDesiredDensity);
                    }

                    TieredScore ts = {harmonic, continuity, densityScore};
                    if (ts > bestScore) {
                        bestScore = ts;
                        bestTuple = {gh, gl, in, ts};
                    }
                }
            }
        }

        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = static_cast<uint8_t>(bestTuple.gh);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)]  = static_cast<uint8_t>(bestTuple.gl);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_Inner)]     = static_cast<uint8_t>(bestTuple.inner);

        prevVoices[0] = bestTuple.gh;
        prevVoices[1] = bestTuple.gl;
        prevVoices[2] = bestTuple.inner;
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix


